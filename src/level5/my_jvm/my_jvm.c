#include <string.h>
#include <pthread.h>
#include <errno.h>
#include "connection.h"
#include "instance.h"
#include "module.h"
#include "log.h"
#include "my_jvm.h"
#include "jamvm_wrapper.h"
#include "config.h"
#include "sock_buffer.h"
#include "socket.h"


static
struct myJvmInstance_s {

  char *gnu_classpath;

  char *prj_path;

  char *class_name; 

  char conf_path[PATH_MAX];

  struct myJvm_config_s m_conf ;

  struct client_sock_info_s {

    struct conditions {
      pthread_mutex_t mtx ;
      pthread_cond_t  cond ;
    } __accept ;

    struct ep {
      int valid ;
      struct conditions c ;
      int new_events;
    } __epoll ;

    struct sock_buffer_entry_s sb_entry ;

    int local_listenfd ;

    int hook_jni ;

  } sock_info ;

  pthread_t jvm_worker ;

} g_myJvmInst = {

  .sock_info = {
    .local_listenfd = -1,

    .hook_jni = 0,

    .__accept = {
      .mtx   = PTHREAD_MUTEX_INITIALIZER,
      .cond  = PTHREAD_COND_INITIALIZER,
    },

    .__epoll = {
      .valid = 0,
      .new_events = 0,
      .c = {
        .mtx   = PTHREAD_MUTEX_INITIALIZER,
        .cond  = PTHREAD_COND_INITIALIZER,
      },
    },

  },
};


static struct module_struct_s g_module;


int my_jvm_rx(Network_t net, connection_t pconn) 
{
  char *inb  = 0;
  size_t sz = 0L;
  struct client_sock_info_s *ps = &g_myJvmInst.sock_info ;
  sbuf_t psb= get_sock_buffer(&ps->sb_entry,pconn->fd);


  if (!psb) {
    log_error("fatal: client fd %d 's NOT hook\n",pconn->fd);
    return 0;
  }


  inb = dbuffer_ptr(pconn->rxb,0);
  sz  = dbuffer_data_size(pconn->rxb) ;

  // no data to be read
  if (sz==0) {
    return 0;
  }


  // change the sock buffer
  pthread_mutex_lock(&psb->mtx);

  psb->in = append_dbuffer(psb->in,inb,sz);

  pthread_cond_signal(&psb->cond);

  pthread_mutex_unlock(&psb->mtx);


  // send epoll notify
  if (ps->__epoll.valid) {
    pthread_mutex_lock(&ps->__epoll.c.mtx);

    ps->__epoll.new_events = 1;

    pthread_cond_signal(&ps->__epoll.c.cond);

    pthread_mutex_unlock(&ps->__epoll.c.mtx);
  }


  dbuffer_lseek(pconn->rxb,sz,SEEK_CUR,0);


#if 0
  if (sz>0)
    log_debug("rx %zu bytes fd %d\n",sz,pconn->fd);
#endif

  return 0;
}

int my_jvm_tx(Network_t net, connection_t pconn)
{
  pconn->l4opt.tx(net,pconn);

#if 0
   log_error("fd %d tx %zu bytes\n",pconn->fd,
            dbuffer_data_size(pconn->txb));
#endif

  return 0;
}

static 
int __my_jvm_do_close(int fd)
{
  struct client_sock_info_s *ps = &g_myJvmInst.sock_info ;
  Network_t net = get_current_net();
  sbuf_t sb = get_sock_buffer(&ps->sb_entry,fd);


  if (!sb) {
    //log_error("sbuf for fd %d not found\n",fd);
    return 0;
  }

  /*
   * when java applications call 'close' , the hook_close() 
   *   will be invoked
   */

  // release client connection from framework
  if (net)
    net->unreg_all(net,sb->conn);

  // release the waiting reader
  pthread_mutex_lock(&sb->mtx);

  set_sock_buffer_valid(sb,0);

  // ok, the sock buffer's changed, notify 
  //  the waiters
  //pthread_cond_signal(&sb->cond);
  pthread_cond_broadcast(&sb->cond);

  pthread_mutex_unlock(&sb->mtx);

  // do 'real' delete sock buffer here
  drop_sock_buffer(&ps->sb_entry,fd);


  log_debug("client fd %d close\n",fd);

  return 0;
}

void my_jvm_close(Network_t net,connection_t pconn)
{
  __my_jvm_do_close(pconn->fd);
}

static
int parse_cmd_line(int argc, char *argv[])
{
  for (int i=1; i<argc; i++) {
    if (!strcmp(argv[i],"-cj")) {
      strcpy(g_myJvmInst.conf_path,argv[i+1]);
    }
    else if (!strcmp(argv[i],"-h")) {
      printf("module '%s' help message\n",THIS_MODULE->name);
      printf("-cj: configure file\n");
      return 1;
    }
  }
  return 0;
}

static 
void my_jvm_dump_params()
{
  log_info("module '%s' param list: =================\n",THIS_MODULE->name);
  log_info("gnu class path dir: %s\n",g_myJvmInst.gnu_classpath);
  log_info("project dir: %s\n",g_myJvmInst.prj_path);
  log_info("project class name: %s\n",g_myJvmInst.class_name);
  if (g_myJvmInst.sock_info.hook_jni==1) {
    char *host = (char*)get_bind_address(&g_myJvmInst.m_conf);
    int port = get_listen_port(&g_myJvmInst.m_conf);

    log_info("bind %s:%d in framework\n",host,port);
  }
  else {
    log_info("bind address:port within webapp\n");
  }
  log_info("module param list end =================\n");
}

void* jvm_task(void *param)
{
  jamvm_wrapper_run(g_myJvmInst.gnu_classpath,g_myJvmInst.prj_path,
                    g_myJvmInst.class_name);

  log_error("fatal: end launching Jamvm!\n");

  return 0;
}

int my_jvm_pre_init(int argc, char *argv[])
{
  if (parse_cmd_line(argc,argv))
    return 1;

  if (init_config(&g_myJvmInst.m_conf,g_myJvmInst.conf_path)) {
    log_error("init config fail\n");
    return -1;
  }

  g_myJvmInst.gnu_classpath = (char*)get_gnu_classpath(&g_myJvmInst.m_conf) ;

  g_myJvmInst.prj_path      = (char*)get_prj_path(&g_myJvmInst.m_conf) ;

  g_myJvmInst.class_name    = (char*)get_prj_class(&g_myJvmInst.m_conf) ;


  // the server address is bind/listen within framework, so 
  //  we can have more Jamvm instances to process user requests
  {
    int port = get_listen_port(&g_myJvmInst.m_conf);
    int *pfd = &g_myJvmInst.sock_info.local_listenfd ;
    char host[32] = "";


    get_bind_address(&g_myJvmInst.m_conf,host);

    if (port<=0 || strlen(host)==0L || (*pfd=new_tcp_svr(__net_atoi(host),port))<=0)
      g_myJvmInst.sock_info.hook_jni = 0;
    else 
      g_myJvmInst.sock_info.hook_jni = 1;
  }

  return 0;
}

int my_jvm_init(Network_t net)
{
  init_sock_buffer_entry(&g_myJvmInst.sock_info.sb_entry);

  // start the jvm worker
  pthread_create(&g_myJvmInst.jvm_worker,NULL,jvm_task,NULL);

  return 0;
}

void my_jvm_release()
{
}


static
int my_jvm_fake_greeting(Network_t net, connection_t pconn)
{
  struct client_sock_info_s *ps = &g_myJvmInst.sock_info ;
  sbuf_t psb = create_sock_buffer(&ps->sb_entry, pconn->fd, net, pconn);


  if (unlikely(!psb)) 
    return -1;


  pthread_mutex_lock(&ps->__accept.mtx);

  sbufq_push(&ps->sb_entry,psb);

  pthread_cond_signal(&ps->__accept.cond);

  pthread_mutex_unlock(&ps->__accept.mtx);



  log_debug("client fd %d accept\n",pconn->fd);

  return 1;
}

static
int my_jvm_hook_accept()
{
  struct client_sock_info_s *ps = &g_myJvmInst.sock_info ;
  int clientfd = -1;


  pthread_mutex_lock(&ps->__accept.mtx);


  /* 
   * FIXME: SHOULD NOT return till clientfd is valid, or 
   *   the Java_gnu_java_nio_VMChannel_accept() should throws an exception
   */
  while (1) {

    if ((clientfd=sbufq_pop(&ps->sb_entry))>0) 
      break ;

    pthread_cond_wait(&ps->__accept.cond,&ps->__accept.mtx);
  }

  pthread_mutex_unlock(&ps->__accept.mtx);

  log_debug("last client: %d\n",clientfd);

  // 'accept' nothing, just return latest client fd
  return clientfd;
}

static
int my_jvm_hook_listen(int listenfd)
{
  Network_t net = get_current_net();
  int fd = g_myJvmInst.sock_info.hook_jni==1?
           g_myJvmInst.sock_info.local_listenfd:listenfd ;

  
  set_nonblock(fd);

  net->reg_local(net,fd,THIS_MODULE->id);

  return 0;
}

static 
int my_jvm_hook_read(int fd, char *buf, size_t *sz)
{
  struct client_sock_info_s *ps = &g_myJvmInst.sock_info ;
  sbuf_t psb = get_sock_buffer(&ps->sb_entry,fd);


  if (!psb) {
    //log_debug("fd %d is NOT my jamvm client\n",fd);
    //errno  = ENOENT ;
    return -2 ;
  }


  pthread_mutex_lock(&psb->mtx);

  /*
   * FIXME: there're one more threads invoke my_jvm_-
   *  -hook_read() with the same fd concurrently, if
   *  __my_jvm_do_close() is called, at least one th-
   *  -read will block at pthread_cond_wait() call
   */
#if 0
  while (1) {

    psb = get_sock_buffer(&ps->sb_entry,fd);
    if (!is_sock_buffer_valid(psb))
      break ;

    if (dbuffer_data_size(psb->in)>0)
      break ;

    pthread_cond_wait(&psb->cond,&psb->mtx);
  }
#else
    //log_debug("[%lx]try wait bytes fd %d\n",pthread_self(),fd);

    psb = get_sock_buffer(&ps->sb_entry,fd);

    if (is_sock_buffer_valid(psb) && dbuffer_data_size(psb->in)==0)
      pthread_cond_wait(&psb->cond,&psb->mtx);
#endif


  if (is_sock_buffer_valid(psb)) {

    read_dbuffer(psb->in,buf,sz);

    log_debug("client fd %d read %zu bytes\n",fd,*sz);
  }
  else {

    *sz = 0;

    log_debug("client fd %d maybe closed\n",fd);
  }


  if (psb)
    pthread_mutex_unlock(&psb->mtx);


  return *sz;
}

static 
int my_jvm_hook_write(int fd, char *buf, size_t sz)
{
  struct client_sock_info_s *ps = &g_myJvmInst.sock_info ;
  sbuf_t psb = get_sock_buffer(&ps->sb_entry,fd);
  connection_t pconn = 0;
  Network_t net = 0;


  if (!psb) {
    //log_debug("fd %d is NOT my jamvm client\n",fd);
    //errno  = ENOENT ;
    return -2 ;
  }


  net   = psb->net ;
  pconn = psb->conn ;
  pconn->txb = append_dbuffer(pconn->txb,buf,sz);

  my_jvm_tx(net,pconn);
  //log_debug("fd %d try tx %zu bytes\n",fd,sz);

  return sz;
}

static
int my_jvm_hook_jni()
{
  // return values: 
  //  0 the server socket's being processed within java application
  //  1 process server's sockets in this framework
  return g_myJvmInst.sock_info.hook_jni;
}

static int my_jvm_hook_close(int fd)
{
  return __my_jvm_do_close(fd);
}

#if 0
static int my_jvm_hook_epoll_create()
{
  Network_t net = get_current_net();
  struct client_sock_info_s *ps = &g_myJvmInst.sock_info ;


  ps->__epoll.valid = 1;

  return net?net->m_efd:-1;
}

static 
int my_jvm_hook_epoll_wait(struct epoll_event *events,int num_events)
{
  sbuf_t pos,n;
  int e=0;
  struct client_sock_info_s *ps = &g_myJvmInst.sock_info ;


  if (ps->__epoll.valid==0)
    return 0;

  if (num_events<=0)
    return -1;


  log_debug("waiting new events...\n");

  pthread_mutex_lock(&ps->__epoll.c.mtx);

  if (ps->__epoll.new_events==0)
    pthread_cond_wait(&ps->__epoll.c.cond,&ps->__epoll.c.mtx);

  ps->__epoll.new_events = 0;

  rbtree_postorder_for_each_entry_safe(pos,n,&ps->sb_entry.u.root,node) {

    if (e>=num_events)
      break ;

    events[e].data.fd = pos->fd ;
    events[e].events |= EPOLLIN ;
    e++ ;
  }

  pthread_mutex_unlock(&ps->__epoll.c.mtx);

  log_debug("return %d events\n",e);

  return e;
}
#endif

static
struct my_jvm_hook_s {
  int (*hook_accept) (void);

  int (*hook_listen)(int);

  int (*hook_read)(int,char*,size_t*);

  int (*hook_write)(int,char*,size_t);

  int (*hook_jni)(void) ;

  int (*hook_close)(int fd);

  /*
   * FIXME: the Jamvm exited abnormally when running 
   *  with 'eventpoll' facility, don't know why
   */
#if 0
  int (*hook_epoll_create)(void);

  int (*hook_epoll_wait)(struct epoll_event *events,int num_events);
#endif

} g_my_jvm_hook = {

  .hook_accept = my_jvm_hook_accept,

  .hook_listen = my_jvm_hook_listen,

  .hook_read   = my_jvm_hook_read,

  .hook_write  = my_jvm_hook_write,
  
  .hook_jni    = my_jvm_hook_jni,

  .hook_close  = my_jvm_hook_close,

#if 0
  .hook_epoll_create  = my_jvm_hook_epoll_create,

  .hook_epoll_wait    = my_jvm_hook_epoll_wait,
#endif
} ;

static 
struct module_struct_s g_module = {

  .name = "my_jvm",

  .id = -1,

  .dyn_handle = NULL,

  .ssl = false,

  .opts[inbound_l5] = {
    .init     = my_jvm_init,
    .release  = my_jvm_release,
    .rx       = my_jvm_rx,
    .tx       = my_jvm_tx,
    .close    = my_jvm_close,
    .greeting = my_jvm_fake_greeting,
  },

  .extra = &g_my_jvm_hook,

};



void my_jvm_module_init(int argc, char *argv[])
{
  if (my_jvm_pre_init(argc,argv))
    return ;

  my_jvm_dump_params();

  register_module(THIS_MODULE);
}
//MODULE_INIT(my_jvm_module_init)

void my_jvm_module_exit()
{
  my_jvm_release();
}
//MODULE_EXIT(my_jvm_module_exit)

