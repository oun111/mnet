#include "instance.h"
#include <sys/inotify.h>
#include <sys/stat.h>
#include <string.h>
#include "action.h"
#include "module.h"
#include "log.h"
#include "mfile.h"
#include "config.h"
//#include "hsyncd.h"

static
struct hsyncd_info_s {
  int in_fd ;

  int wd ;

  char monitor_path[PATH_MAX];

  struct monitor_file_entry_s m_mfEntry ;

  struct hsyncd_config_s m_conf ;

  char conf_path[PATH_MAX];

  char last_move_from[PATH_MAX];

} g_hsdInfo = {
  .last_move_from = "\0",
};


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

static
int hsyncd_rx(Network_t net, connection_t pconn)
{
  struct stat st; 
  char fb[PATH_MAX] = "", *file = fb; 
  char* buf = dbuffer_ptr(pconn->rxb,0) ; 
  const char *base_path = g_hsdInfo.monitor_path;
  int ln = dbuffer_data_size(pconn->rxb), ret=0;
  struct inotify_event *event = NULL;


  // no data to process
  if (ln<=0) {
    return 0;
  }

	for (int offs=0; offs<ln; offs+=(sizeof(struct inotify_event)+event->len)) {

    event = (struct inotify_event*)(buf+offs) ;

    if (event->mask&IN_ISDIR)
      continue ;


    snprintf(file,sizeof(fb),"%s/%s",base_path,event->name);

    if (stat(file,&st)) {
      log_debug("cant get file %s stat\n",file);
    }   

    if (event->mask&IN_CREATE || event->mask&IN_MODIFY) {
      ret = load_mfile(&g_hsdInfo.m_mfEntry,file,st.st_size);

      log_debug("monitor file '%s', size %ld, ret %d\n",file,st.st_size,ret);
    }   

    else if (event->mask&IN_MOVED_FROM) {
      strcpy(g_hsdInfo.last_move_from,file);

      log_debug("the %s %s is moved from\n",event->mask&IN_ISDIR?"dir":"file",file);
    }   

    else if (event->mask&IN_MOVED_TO) {

      if (strlen(g_hsdInfo.last_move_from)>0) {

        ret = rename_mfile(&g_hsdInfo.m_mfEntry,g_hsdInfo.last_move_from,file);
        g_hsdInfo.last_move_from[0] = '\0';

        log_debug("monitor file '%s' is renamed to '%s',ret: %d\n",g_hsdInfo.last_move_from,file,ret);
      }

      log_debug("the %s %s is moved to\n",event->mask&IN_ISDIR?"dir":"file",file);
    }   
#if 0
    else if (event->mask&IN_MOVE_SELF) {
      log_debug("the %s %s is moved self\n",event->mask&IN_ISDIR?"dir":"file",file);
    }   
#endif

	} 

  dbuffer_lseek(pconn->rxb,ln,SEEK_CUR,0);

  return 0;
}

static
void hsyncd_release()
{
  inotify_rm_watch(g_hsdInfo.in_fd,g_hsdInfo.wd);

  close(g_hsdInfo.in_fd);

  release_all_mfiles(&g_hsdInfo.m_mfEntry);
}

static
void hsyncd_close(Network_t net, connection_t pconn)
{
  net->unreg_all(net,pconn);

  close(pconn->fd);
}

static
struct module_struct_s g_module = {

  .name = "hbase syncd",

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

static int hsyncd_pre_init(const char *mpath)
{
  //int v = inotify_init1(IN_NONBLOCK);
  int v = inotify_init();

  if (v<0) {
    log_error("inotify_init1() fail!\n");
    return -1;
  }

  g_hsdInfo.in_fd = v ;


  // TODO: add watch path list
  v = inotify_add_watch(g_hsdInfo.in_fd,mpath,
                        IN_CREATE|IN_MODIFY|IN_MOVED_FROM|IN_MOVED_TO/*|IN_MOVE_SELF*/);

  if (v<0) {
    log_error("inotify_add_watch() fail!\n");
    return -1;
  }

  g_hsdInfo.wd = v ;

  strncpy(g_hsdInfo.monitor_path,mpath,sizeof(g_hsdInfo.monitor_path));

  // the monitor file entry
  init_mfile_entry(&g_hsdInfo.m_mfEntry,6,-1);

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
  char *tmp = g_hsdInfo.monitor_path;

  log_info("module '%s' param list: =================\n",THIS_MODULE->name);
  log_info("monitor path dir: %s\n",tmp);
  log_info("module param list end =================\n");
}

void hsyncd_module_init(int argc, char *argv[])
{
  if (parse_cmd_line(argc,argv)) {
    return ;
  }

  if (init_config(&g_hsdInfo.m_conf,g_hsdInfo.conf_path)) {
    log_error("init config fail\n");
    return ;
  }

  if (hsyncd_pre_init(get_monitor_path(&g_hsdInfo.m_conf))) {
    return ;
  }

  dump_params();

  register_module(THIS_MODULE);

  set_proc_name(argc,argv,"hsyncd");
}

void hsyncd_module_exit()
{
  log_debug("invoked\n");
}

