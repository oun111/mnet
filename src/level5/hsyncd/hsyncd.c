#include "instance.h"
#include <sys/inotify.h>
#include <sys/stat.h>
#include <string.h>
#include "action.h"
#include "module.h"
#include "log.h"
#include "mfile.h"
//#include "hsyncd.h"

static
struct hsyncd_info_s {
  int in_fd ;

  int wd ;

  char watch_path[PATH_MAX];

  struct monitor_file_entry_s m_mfEntry ;

} g_hsdInfo ;


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
  const char *base_path = g_hsdInfo.watch_path;
  int ln = dbuffer_data_size(pconn->rxb);


  // no data to process
  if (ln<=0) {
    return 0;
  }

	for (int offs = 0;offs<ln;) {

    struct inotify_event *event = (struct inotify_event*)(buf+offs) ;

		snprintf(file,sizeof(fb),"%s/%s",base_path,event->name);

		if (stat(file,&st)) {
			log_debug("cant get file %s stat\n",file);
		}   

		if (event->mask&IN_CREATE) {
			log_debug("the %s %s is created\n",event->mask&IN_ISDIR?"dir":"file",file);
		}   

		else if (event->mask&IN_MODIFY) {
			log_debug("the %s %s is modify, size: %ld\n",event->mask&IN_ISDIR?"dir":"file",file,st.st_size);
		}   

		else if (event->mask&IN_MOVED_FROM) {
			log_debug("the %s %s is moved from\n",event->mask&IN_ISDIR?"dir":"file",file);
		}   

		else if (event->mask&IN_MOVED_TO) {
			log_debug("the %s %s is moved to\n",event->mask&IN_ISDIR?"dir":"file",file);
		}   

		else if (event->mask&IN_MOVE_SELF) {
			log_debug("the %s %s is moved self\n",event->mask&IN_ISDIR?"dir":"file",file);
		}   

		offs += sizeof(struct inotify_event)+event->len ;
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

static int hsyncd_pre_init(const char *wpath)
{
  //int v = inotify_init1(IN_NONBLOCK);
  int v = inotify_init();

  if (v<0) {
    log_error("inotify_init1() fail!\n");
    return -1;
  }

  g_hsdInfo.in_fd = v ;


  // TODO: add watch path list
  v = inotify_add_watch(g_hsdInfo.in_fd,wpath,
                        IN_CREATE|IN_MODIFY|IN_MOVED_FROM|IN_MOVED_TO|IN_MOVE_SELF);

  if (v<0) {
    log_error("inotify_add_watch() fail!\n");
    return -1;
  }

  g_hsdInfo.wd = v ;

  strncpy(g_hsdInfo.watch_path,wpath,sizeof(g_hsdInfo.watch_path));

  // the monitor file entry
  init_mfile_entry(&g_hsdInfo.m_mfEntry,-1,-1);

  return 0;
}

void hsyncd_module_init(int argc, char *argv[])
{
  // TODO: process arguments/configs

  // TODO: get watch-path from config
  if (hsyncd_pre_init("/tmp/")) {
    return ;
  }

  register_module(THIS_MODULE);

  set_proc_name(argc,argv,"hsyncd");
}

void hsyncd_module_exit()
{
  log_debug("invoked\n");
}

