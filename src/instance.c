#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <sys/wait.h>
#include "instance.h"
#include "proto.h"
#include "socket.h"
#include "mm_porting.h"
#include <sys/prctl.h>
#include "L4.h"
#include "log.h"
#include "module.h"
#include "timer.h"


DECLARE_LOG;



static connection_t register_local_protocol(Network_t net, int fd, int mod_id) ;

static connection_t register_inbound_protocol(Network_t net, int fd, int mod_id);

static connection_t register_outbound_protocol(Network_t net, int fd, int mod_id);

static int unregister_protocols(Network_t net, connection_t pconn);

static
struct __attribute__((__aligned__(64))) main_instance_s {

  char name[32];

  int is_master;

  size_t num_workers;

  struct Network_s g_nets ;

  int worker_stop:1 ;

  int use_log:1 ;

  int exit ;

  struct simple_timer_entry_s timers ;
} 
g_inst =
{
  .num_workers    = 0,

  .g_nets = {
    .reg_local    = register_local_protocol,
    .reg_inbound  = register_inbound_protocol,
    .reg_outbound = register_outbound_protocol,
    .unreg_all    = unregister_protocols,
  },

  .is_master      = 0,

  .worker_stop    = 0,

  .use_log        = 0,

  .exit           = 0,
};


static 
struct simple_timer_s g_log_flusher =
{
  .desc = "log flusher",

  .cb = flush_log,

  .timeouts = 5,
} ;

static
struct simple_timer_s g_tos_conn_killer = 
{
  .desc = "timeout connection killer",

  .cb = scan_timeout_conns,

  .timeouts = 86400,
} ;


connection_t add_to_event_poll(Network_t net, int fd, proto_opt *l4opt, 
                               proto_opt *l5opt, bool bSSL, bool markActive)
{
  connection_t pconn = alloc_conn(net,fd,l4opt,l5opt,bSSL,markActive);


  if (!pconn) {
    log_error("no connection item for fd %d avaiable\n",fd);
    return NULL;
  }

  add_to_epoll(net->m_efd,fd,pconn);

  log_debug("client %d connect\n",pconn->fd);

  return pconn;
}

void del_from_event_poll(Network_t net, connection_t pconn)
{
  if (!pconn->is_close) {

    log_debug("client %d disconnect\n",pconn->fd);

    del_from_epoll(net->m_efd, pconn->fd);

    free_conn(net,pconn);
  }
}

static inline void do_close(Network_t net, connection_t pconn)
{
  log_debug("closing client %d\n",pconn->fd);
  pconn->l5opt.close(net,pconn);
  pconn->l4opt.close(net,pconn);
}

static inline int do_tx(Network_t net, connection_t pconn)
{
  int ret = 0;

  do {
    ret = pconn->l4opt.tx(net,pconn);

  } while (!ret);

  return ret==1?0:ret ;
}

static inline int do_rx(Network_t net, connection_t pconn)
{
  int ret = 0, ret1 = 0;
    
  while (!(ret1 || ret)) {
    ret = pconn->l4opt.rx(net,pconn);
    ret1= pconn->l5opt.rx(net,pconn) ;
  }

  return (ret<0||ret1<0)?-1:0 ;
}

void* instance_event_loop()
{
  Network_t net = &g_inst.g_nets ;


  while (!(g_inst.worker_stop&1)) {

    int nEvents = epoll_wait(net->m_efd,net->elist,MAXEVENTS,-1);

    for (int i=0;i<nEvents;i++) {

      connection_t pconn = (connection_t)(net->elist[i].data.ptr);
      int event = net->elist[i].events;

      if (pconn->is_close & 1)
        continue ;

      if ((event & EPOLLERR) || (event & EPOLLHUP) || (event & EPOLLRDHUP)) {
        do_close(net,pconn);
      }
          
      else { 
        /* request in */
        if (event & EPOLLIN) {
          if (do_rx(net,pconn)<0) {
            do_close(net,pconn);
            continue ;
          } 
          update_conn_times(pconn);
        }   
        /* ready to send */
        if (event & EPOLLOUT) {
          disable_send(net->m_efd,pconn->fd,pconn);
          if (do_tx(net,pconn)<0) {
            do_close(net,pconn);
            continue ;
          }
          update_conn_times(pconn);
        }
      }

    } // end for


  }

  return 0;
}

static
void dump_instance_params()
{
  log_info("main param list: =================\n");
  log_info("name: %s\n",g_inst.name);
  log_info("num_workers: %zu\n",g_inst.num_workers);
  log_info("log flush interval: %d(s)\n",g_log_flusher.timeouts);
  log_info("connection timeout: %d(s)\n",g_tos_conn_killer.timeouts);
  log_info("main param list ends =================\n");
}

static
int parse_cmd_line(int argc, char *argv[])
{
  for (int i=1; i<argc; i++) {
    // log flush interval
    if (!strcmp(argv[i],"-mF")) {
      g_log_flusher.timeouts = atoi(argv[i+1]);
    }
    // max workers count
    else if (!strcmp(argv[i],"-mW")) {
      g_inst.num_workers = atoi(argv[i+1]);
    }
    // use log output
    else if (!strcmp(argv[i],"-L")) {
      char *logpath = (i+1)<argc?argv[i+1]:"/tmp";

      g_inst.use_log = 1;
#ifdef FRAMEWORKNAME
      init_log(&g_log,logpath,FRAMEWORKNAME);
#else
      init_log(&g_log,logpath,"app");
#endif
    }
    // connection timeout
    else if (!strcmp(argv[i],"-cTo")) {
      g_tos_conn_killer.timeouts = atoi(argv[i+1]);
    }
    else if (!strcmp(argv[i],"-h")) {
      printf("%s help message\n",argv[0]);
      printf("-mF:  <log flush interval>\n");
      printf("-mC:  <max client connections>\n");
      printf("-mW:  <max workers count>\n");
      printf("-cTo: <connection timeouts>\n");
      printf("-L:   <use log output>\n");
      g_inst.exit = 1;
    }
  }
  return 0;
}

void sig_term_handler(int sn)
{
  flush_logs();
  g_inst.worker_stop = 1;
  exit(-1);
}

int instance_start(int argc, char *argv[])
{
  int fd = -1, pid = -1;


  parse_cmd_line(argc,argv);

  init_module_list(argc,argv);

  // just output help messages, don't go further
  if (g_inst.exit==1)
    exit(0);

  dump_instance_params();

  log_info("starting instance...\n");

  /* create children */
  for (int i=0;i<g_inst.num_workers;i++) {
    pid = fork();

    if (pid>0) {
      log_debug("creating worker %d\n",pid);
      g_inst.is_master = 1;
    }
    else if (pid<0)  {
      log_error("error create worker, error: %s\n",strerror(errno));
      g_inst.is_master = 1;
    }
    else {
      g_inst.is_master = 0;
      break ;
    }
  }

  // signals
  signal(SIGTERM,sig_term_handler);
  signal(SIGINT,sig_term_handler);
  signal(SIGSEGV,sig_term_handler);
  signal(SIGPIPE,SIG_IGN);

  save_log_pid();

  if (g_inst.is_master == 0) {

    // timers init
    init_timer_entry(&g_inst.g_nets,&g_inst.timers);
    register_simple_timer(&g_inst.timers,&g_log_flusher);
    register_simple_timer(&g_inst.timers,&g_tos_conn_killer);

    log_debug("worker %d start working\n",g_log.pid);

    /* create thread-based epoll */
    fd = init_epoll();

    if (fd<=0) {
      log_error("init epoll fail: %s\n",strerror(errno));
      return -1;
    }

    init_conn_pool(&g_inst.g_nets,-1);

    g_inst.g_nets.m_efd = fd ;


    // invoke init() within all modules
    module_t pmod = NULL;

    for (int n=0; (pmod=get_module(n)); n++) {

      for (int i=0;i<max_protos;i++) {
        if (pmod->opts[i].init && pmod->opts[i].init(&g_inst.g_nets)) {
          log_error("proto %d init() fail\n",i);
          return -1;
        }
      }
    }

    instance_event_loop();
  }
  else {
    log_debug("pid %d waiting child.....\n",getpid());
    wait(0);
  }

  instance_stop();

  return 0;
}

int instance_stop()
{
  module_t pmod = NULL;


  log_info("releasing resources...\n");

  // invoke release() within all modules
  for (int n=0;(pmod=get_module(n));n++) {

    for (int i=0;i<max_protos;i++) {
      if (pmod->opts[i].release)
        pmod->opts[i].release();
    }
  }

  release_timer_entry(&g_inst.timers);

  release_conn_pool(&g_inst.g_nets);

  release_module_list();

  close_log(&g_log);

  return 0;
}

static 
connection_t register_protocols(Network_t net, int fd, int mod_id, int l4proto, int l5proto)
{
  connection_t pconn = 0;
  proto_opt *l4opt = 0;
  proto_opt *l5opt = 0;
  module_t pmod = get_module(mod_id);


  if (!pmod) {
    log_debug("module id %d invalid\n",mod_id);
    return NULL;
  }

  if (l4proto>=local_l4 && l4proto<max_protos)
    l4opt = &pmod->opts[l4proto];

  if (l5proto>=local_l4 && l5proto<max_protos)
    l5opt = &pmod->opts[l5proto];

  pconn = add_to_event_poll(net,fd,l4opt,l5opt,pmod->ssl,l5proto!=-1);
  if (pconn)
    pconn->module_id = mod_id ;

  //log_debug("fd %d modid %d (%s) done!\n",fd,mod_id,pmod->name);

  return pconn ;
}

static
connection_t register_local_protocol(Network_t net, int fd, int mod_id)
{
  return register_protocols(net,fd,mod_id,local_l4,-1) ;
}

static
connection_t register_inbound_protocol(Network_t net, int fd, int mod_id)
{
  return register_protocols(net,fd,mod_id,normal_l4,inbound_l5) ;
}

static
connection_t register_outbound_protocol(Network_t net, int fd, int mod_id)
{
  return register_protocols(net,fd,mod_id,normal_l4,outbound_l5) ;
}

static 
int unregister_protocols(Network_t net, connection_t pconn)
{
  del_from_event_poll(net,pconn);

  return 0;
}

int get_cpu_cores(void)
{
  char buf[32];
  FILE *ps = popen("cat /proc/cpuinfo|grep processor|wc -l","r");

  fgets(buf,32,ps);
  pclose(ps);
  return atoi(buf);
}

Network_t get_current_net()
{
  return &g_inst.g_nets;
}

void add_external_timer(simple_timer_t t)
{
  register_simple_timer(&g_inst.timers,t);
}

void set_proc_name(int argc, char *argv[], const char *newname)
{
  char *lastp = argv[argc-1] + strlen(argv[argc-1]);
  size_t sz = lastp-argv[0];


  bzero(argv[0],sz);

  snprintf(argv[0],sz,"%s",newname);

  prctl(PR_SET_NAME,argv[0]);
}

