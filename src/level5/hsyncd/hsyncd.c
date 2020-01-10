#include "instance.h"
#include <sys/inotify.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include "action.h"
#include "module.h"
#include "log.h"
#include "mfile.h"
#include "config.h"
#include "wdcache.h"
#include "formats.h"
#include "myrbtree.h"
#include "processor.h"
#include "rpc.h"

static
struct hsyncd_info_s {
  int in_fd ;

  struct wdcache_entry_s m_wdEntry ;  

  struct monitor_file_entry_s m_mfEntry ;

  struct hsyncd_config_s m_conf ;

  struct formats_entry_s m_fmtEntry ;

  char conf_path[PATH_MAX];

  char last_move_from[PATH_MAX];

  struct list_head prio_list ;

  struct rpc_data_s rpc_buf ;

  bool is_parent ;

} g_hsdInfo = {

  .last_move_from = "\0",

  .is_parent = true,

};

struct rpcClnt_object_s {

  struct {
    struct rpc_clnt_s clnt ;
  } worker ;

  struct list_head pri_item ;
} ;
typedef struct rpcClnt_object_s* rpcClnt_object_t ;


static int hsyncd_init(Network_t net);


static
int hsyncd_l4_rx(Network_t net, connection_t pconn)
{
  const size_t rx_size = sizeof(struct inotify_event)*10 ;
  int infd = g_hsdInfo.in_fd ;
  char* buf = 0 ; 
	int ln = 0;


  pconn->rxb = rearrange_dbuffer(pconn->rxb,rx_size);
  buf = dbuffer_ptr(pconn->rxb,1) ; 
	ln  = read(infd,buf,rx_size);

  if (ln<=0) {
    return /*-1*/0;
  }

  //log_debug("rx size: %d\n",ln);

  /* update write pointer */
  dbuffer_lseek(pconn->rxb,ln,SEEK_CUR,1);

  return 0;
}

static int rpc_send(common_format_t cf)
{
  cf_pair_t pos;
  struct rpc_data_s *pRpcd = &g_hsdInfo.rpc_buf ;
  rpcClnt_object_t ph = list_first_entry(&g_hsdInfo.prio_list, 
                       struct rpcClnt_object_s,pri_item);


  strncpy(pRpcd->table,cf->tbl,sizeof(pRpcd->table));
  strncpy(pRpcd->rowno,cf->row,sizeof(pRpcd->rowno));

  list_for_each_entry(pos,&cf->cf_list,upper) {

    strncpy(pRpcd->k,pos->cf,sizeof(pRpcd->k));
    strncpy(pRpcd->v,pos->val,sizeof(pRpcd->v));

    rpc_clnt_tx(&ph->worker.clnt,pRpcd);
  }

  list_del(&ph->pri_item);
  list_add_tail(&ph->pri_item,&g_hsdInfo.prio_list);

  return 0;
}

static int init_rpc_clients(size_t count)
{
  // priority list
  INIT_LIST_HEAD(&g_hsdInfo.prio_list);

  for (int i=0;i<count;i++) {
    rpcClnt_object_t ph = kmalloc(sizeof(struct rpcClnt_object_s),0L);

    // init rpc client
    rpc_clnt_init(&ph->worker.clnt,i);

    // join priority list
    list_add(&ph->pri_item,&g_hsdInfo.prio_list);
  }

  return 0;
}

static void release_rpc_clients()
{
  rpcClnt_object_t pos,n ;

  list_for_each_entry_safe (pos,n,&g_hsdInfo.prio_list,pri_item) {
    rpc_clnt_release(&pos->worker.clnt);
    kfree(pos);
  }
}

static int read_format_data(formats_entry_t entry, int fmt_id, dbuffer_t inb)
{
  common_format_t fdata = 0;
  formats_cb_t pc = get_format(entry,fmt_id);


  if (!pc || !pc->parser) {
    log_error("no parser register for format %d\n",fmt_id);
    return -1;
  }

  while (pc->parser(inb,&fdata)==1) {
    if (fdata) {
      rpc_send(fdata);
      free_common_format(fdata);
    }
  }

  return 0;
}

static int process_format_data(const char *file, int fmt_id)
{
  mfile_t pf ;
  struct stat st; 
  int ret = 0;


  // get file size
  if (stat(file,&st)) 
    return -1;

  ret = load_mfile(&g_hsdInfo.m_mfEntry,file,st.st_size,&pf);

  // parse mfile
  if (ret>0) {
    dbuffer_t inb = get_mfile_contents(pf);

    read_format_data(&g_hsdInfo.m_fmtEntry,fmt_id,inb);
  }

  return 0;
}

static
int hsyncd_rx(Network_t net, connection_t pconn)
{
  char fb[PATH_MAX] = "", *file = fb; 
  char* buf = dbuffer_ptr(pconn->rxb,0) ; 
  int ln = dbuffer_data_size(pconn->rxb);
  struct inotify_event *event = NULL;


  // no data to process
  if (ln<=0) {
    return 0;
  }

	for (int offs=0,ret=0; offs<ln; offs+=(sizeof(struct inotify_event)+event->len),ret=0) {

    event = (struct inotify_event*)(buf+offs) ;

    if (event->mask&IN_ISDIR)
      continue ;

    wdcache_t pw = get_wdcache(&g_hsdInfo.m_wdEntry,event->wd);
    if (!pw) {
      log_error("get no watch path by wd %d\n",event->wd);
      continue ;
    }

    const char *base_path = (char*)get_wdcache_path(pw);
    snprintf(file,sizeof(fb),"%s/%s",base_path,event->name);

    if (event->mask&IN_MOVED_FROM) {
      strcpy(g_hsdInfo.last_move_from,file);
    }   

    else if (event->mask&IN_MOVED_TO) {

      if (strlen(g_hsdInfo.last_move_from)>0) {
        char *lf = g_hsdInfo.last_move_from;

        ret = rename_mfile(&g_hsdInfo.m_mfEntry,lf,file);

        log_debug("file '%s' is renamed to '%s',ret: %d\n",lf,file,ret);
        *lf = '\0';
      }
    }   

    if (event->mask&IN_CREATE || event->mask&IN_MODIFY || 
         event->mask&IN_MOVED_TO) {

      process_format_data(file,get_wdcache_fmtid(pw));
    }

	} 

  dbuffer_lseek(pconn->rxb,ln,SEEK_CUR,0);

  return 0;
}

static
void hsyncd_release()
{
}

static
void hsyncd_close(Network_t net, connection_t pconn)
{
  net->unreg_all(net,pconn);

  close(pconn->fd);
}

static
struct module_struct_s g_module = {

  .name = "hsyncd",

  .id = -1,

  .dyn_handle = NULL,

  .ssl = false,

  .opts[normal_l4] = {
    .rx = hsyncd_l4_rx,
    .close = hsyncd_close,
  },

  .opts[inbound_l5] = {
    .rx      = hsyncd_rx,
    .init    = hsyncd_init,
    .release = hsyncd_release,
    .close   = hsyncd_close,
  },
} ;


static
int hsyncd_init(Network_t net)
{
  net->reg_inbound(net,g_hsdInfo.in_fd,THIS_MODULE->id);

  return 0;
}

static int add_watch_paths(hsyncd_config_t conf)
{
  tm_item_t pos, n;
  //tm_item_t pos1, n1;
  tree_map_t pl = get_tree_map_nest(conf->m_globSettings.monitor_paths,"monitorPaths");


  MY_RBTREE_PREORDER_FOR_EACH_ENTRY_SAFE(pos,n,&pl->u.root,node) {
    tree_map_t spath = pos->nest_map ;

    char *mpath = get_tree_map_value(spath,"path");
    char *fmtid = get_tree_map_value(spath,"formatId");

    int wd = inotify_add_watch(g_hsdInfo.in_fd,mpath,IN_CREATE|
                               IN_MODIFY|IN_MOVED_FROM|IN_MOVED_TO/*|IN_MOVE_SELF*/);
    if (wd<0) {
      log_error("inotify_add_watch() fail!\n");
      continue;
    }

    // save wd -> watch path mapping
    add_wdcache(&g_hsdInfo.m_wdEntry,wd,mpath,atoi(fmtid));
  }
  return 0;
}

static int preload_monitor_paths(hsyncd_config_t conf)
{
  tm_item_t pos, n;
  char abpath[PATH_MAX]="";
  tree_map_t pl = get_tree_map_nest(conf->m_globSettings.monitor_paths,
                                    "monitorPaths");

  MY_RBTREE_PREORDER_FOR_EACH_ENTRY_SAFE(pos,n,&pl->u.root,node) {
    tree_map_t spath = pos->nest_map ;
    char *mpath = get_tree_map_value(spath,"path");
    char *fmtid = get_tree_map_value(spath,"formatId");
    DIR *dir = opendir(mpath);
    struct dirent *ent = 0;


    snprintf(abpath,sizeof(abpath),"%s",mpath);

    while (dir && (ent=readdir(dir))) {
      if (ent->d_type&DT_DIR)
        continue ;

      snprintf(abpath+strlen(mpath),sizeof(abpath)-strlen(mpath),
               "/%s",ent->d_name);

      process_format_data(abpath,atoi(fmtid));
    }

    closedir(dir);
  }

  log_debug("preload monitor path(s) done!");

  return 0;
}

static int hsyncd_pre_init(hsyncd_config_t conf, int workers)
{
  //int v = inotify_init1(IN_NONBLOCK);
  int v = inotify_init();


  if (v<0) {
    log_error("inotify_init1() fail!\n");
    return -1;
  }

  g_hsdInfo.in_fd = v ;

  // init all file formats
  init_formats_entry(&g_hsdInfo.m_fmtEntry);

  // the wd cache
  init_wdcache_entry(&g_hsdInfo.m_wdEntry);

  // the monitor file entry
  init_mfile_entry(&g_hsdInfo.m_mfEntry,6,-1);

  if (init_rpc_clients(workers)) {
    return -1;
  }

  if (need_preload(conf)==true) {
    preload_monitor_paths(conf);
  }

  // add watch path list
  add_watch_paths(conf);

  return 0;
}

static
int parse_cmd_line(int argc, char *argv[])
{
  for (int i=1; i<argc; i++) {
    if (!strcmp(argv[i],"-ch")) {
      strcpy(g_hsdInfo.conf_path,argv[i+1]);
    }
    else if (!strcmp(argv[i],"-h")) {
      printf("module '%s' help message\n",THIS_MODULE->name);
      printf("-ch: configure file\n");
      return 1;
    }
  }
  return 0;
}

static 
void dump_params()
{
  //char *tmp = g_hsdInfo.monitor_path;

  log_info("module '%s' param list: =================\n",THIS_MODULE->name);
  //log_info("monitor path dir: %s\n",tmp);
  log_info("module param list end =================\n");
}

void hsyncd_module_init(int argc, char *argv[])
{
  char host[32], procName[96] = "";
  int port = 0, workers = 0;


  if (parse_cmd_line(argc,argv)) {
    return ;
  }

  if (init_config(&g_hsdInfo.m_conf,g_hsdInfo.conf_path)) {
    log_error("init config fail\n");
    return ;
  }

  workers = get_worker_count(&g_hsdInfo.m_conf);
  if (workers<=0) {
    workers = 1 ;
    log_info("force to %d worker\n",workers);
  }

  // create workers
  get_hbase_client_settings(&g_hsdInfo.m_conf,host,sizeof(host),&port);

  for (int i=0;i<workers;i++) {
    int pid = fork();

    if (!pid) {
      snprintf(procName,sizeof(procName),"%s-worker",THIS_MODULE->name);

      proc_main(host,port,i);

      set_proc_name(argc,argv,procName);

      g_hsdInfo.is_parent = false ;

      return ;
    }
  }

  // init is_parent
  if (hsyncd_pre_init(&g_hsdInfo.m_conf,workers)) {
    return ;
  }

  dump_params();

  register_module(THIS_MODULE);

  set_proc_name(argc,argv,THIS_MODULE->name);
}

void hsyncd_module_exit()
{
  if (g_hsdInfo.is_parent) {

    //inotify_rm_watch(g_hsdInfo.in_fd,g_hsdInfo.wd);

    close(g_hsdInfo.in_fd);

    release_all_mfiles(&g_hsdInfo.m_mfEntry);

    release_all_wdcaches(&g_hsdInfo.m_wdEntry);

    release_all_formats(&g_hsdInfo.m_fmtEntry);

    release_rpc_clients();
  }

  free_config(&g_hsdInfo.m_conf);

  log_debug("releasing resource...\n");
}

