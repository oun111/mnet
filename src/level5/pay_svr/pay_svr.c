#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include "pay_action.h"
#include "dbuffer.h"
#include "log.h"
#include "file_cache.h"
#include "instance.h"
#include "socket.h"
#include "module.h"
#include "tree_map.h"
#include "kernel.h"
#include "myrbtree.h"
#include "config.h"
#include "pay_action_list.h"



const char normalHdr[] = "HTTP/1.1 %s\r\n"
                         "Server: pay-svr/0.1\r\n"
                         //"Content-Type: text/html;charset=GBK\r\n"
                         "Content-Type: text/json;charset=GBK\r\n"
                         "Content-Length:%zu      \r\n"
                         "Date: %s\r\n\r\n";



struct pay_svr_conf {
  char host[32];

  int fd;
  int port;

  int notify_fd ;
  int notify_port ;

  char conf_path[PATH_MAX];

  struct paySvr_config_s m_conf;

  pay_action_entry_t m_pas0 ;
  

} g_paySvrConf = {
  .host = "127.0.0.1",
  .port = 4321,
  .fd = -1,
  .notify_fd = -1,

};


static struct pay_svr_keywords_t {
  const char *channel ;

} g_paySvrKeywords = {
  .channel = "channel",

};



static int pay_svr_init(Network_t net);

static int pay_svr_notify_init(Network_t net);

static int parse_cmd_line(int argc, char *argv[]);



void register_pay_action_list(void)
{
#if 1
  for (int i =0; g_payAction_list[i]!=NULL; i++) {
    pay_action_t pa = g_payAction_list[i];
    add_pay_action(g_paySvrConf.m_pas0,pa);
  }
#else
  for (pay_action_t pa =(pay_action_t)g_payAction_list; pa!=NULL; pa++) {
    add_pay_action(g_paySvrConf.m_pas0,pa);
  }
#endif
}

static 
int pay_svr_do_error(connection_t pconn)
{
  const char *bodyPage = "{ \"status\":\"error\" }\r\n" ;
  char hdr[256] = "";
  time_t t = time(NULL);
  char tb[64];

  ctime_r(&t,tb);
  snprintf(hdr,256,normalHdr,"404",strlen(bodyPage),tb);

  pconn->txb = write_dbuffer(pconn->txb,hdr,strlen(hdr));
  pconn->txb = append_dbuffer(pconn->txb,(char*)bodyPage,strlen(bodyPage));

  return 0;
}

static
int uri_to_map(char *strKv, size_t kvLen, tree_map_t entry)
{
  char *sp = (char*)strKv, *p = 0, *e = 0, *tmp=0;


  while (sp<(strKv+kvLen)) {

    p = strchr(sp,'&');
    if (p) 
      *p = '\0';

    e = strchr(sp,'=');
    if (!e) {
      log_error("invalid request format in '%s'\n",strKv);
      return -1;
    }

    // trim
    for (;isspace(*sp);sp++);
    for (tmp=e-1;isspace(*tmp);tmp--);
    size_t kl = tmp-sp+1 ;
    e ++;
    for (;isspace(*e);e++);
    for (tmp=e+strlen(e)-1;isspace(*tmp);tmp--);
    size_t vl = tmp-e+1;

    // put in map
    put_tree_map(entry,sp,kl,e,vl);

    sp += strlen(sp)+1;

    if (p) *p = '&';
  }

  return 0;
}

static int process_param_list(connection_t pconn, char *kvlist, const char *action)
{
  int ret = -1;
  pay_action_t pos ;
  char key[256] = "", *payChan = 0;
  tree_map_t map = new_tree_map();


  //log_debug("kvlist: %s\n",kvlist);
  uri_to_map(kvlist,strlen(kvlist),map);
  payChan = get_tree_map_value(map,(char*)g_paySvrKeywords.channel,
                               strlen(g_paySvrKeywords.channel));
  if (!payChan) {
    log_error("no '%s' param found\n",g_paySvrKeywords.channel);
    goto __end;
  }

  // construct the 'key'
  snprintf(key,256,"%s/%s",payChan,action);

  if ((pos=get_pay_action(g_paySvrConf.m_pas0,key))) {
    ret = pos->cb(pconn,map);
  }

__end:
  delete_tree_map(map);

  return ret;
}

static 
int pay_svr_do_post(connection_t pconn, const dbuffer_t req, const size_t len)
{
  char action[256] = ""/*, e=0*/;
  /* XXX: '/' should be existing */
  dbuffer_t ps = strstr(req,"/");
  dbuffer_t end1 = strstr(ps," ");
  const char kv_start[] = "\r\n\r\n";
  const int kv_len = 4;
  size_t ln = 0;


  // parse action 
  if (*(ps+1)==' ') {
    ln = 1;
  }
  else {
    ln = end1-ps-1 ;
    ps = ps+1 ;
  }
  memcpy(action,ps,ln);
  action[ln] = '\0';
  log_debug("action: %s\n",action);

  // parse key-values
  end1 = req + len ;
  ps = strstr(req,kv_start);
  if (!ps) {
    log_error("invalid header ending\n");
    return -1;
  }

  // no kv list
  if ((ps+kv_len)>=end1) {
    pay_svr_do_error(pconn);
    return 0;
  }

  // have key-values
  ps += kv_len ;
  log_debug("values: %s\n",ps);


  return process_param_list(pconn,ps,action);
}

void pay_svr_print_debug_req(dbuffer_t req, size_t len, int res)
{
  char c = req[len];

  log_debug("%s...(size %zu) - %s\n",req,len,!res?"ok":"fail");
  req[len] = c;
}

static dbuffer_t get_sub_req(dbuffer_t subreq, size_t *len) 
{
#define GET_START_POS(__req) ({\
  dbuffer_t __ptr = strstr((__req),"POST");\
  !__ptr?strstr((__req),"GET"):__ptr; \
})
  dbuffer_t sp = GET_START_POS(subreq), next=NULL;

  if (!sp)
    return NULL ;

  /* get next sub req's begining */
  next = GET_START_POS(subreq+1);

  /* req len */
  *len = !next?strlen(subreq):(next-subreq+1);

  return sp ;
}

int pay_svr_rx(Network_t net, connection_t pconn)
{
  size_t datalen = dbuffer_data_size(pconn->rxb);
  dbuffer_t subreq = 0, next = dbuffer_ptr(pconn->rxb,0);
  int status = 0;
  size_t reqlen = 0L ;


  if (datalen==0) return 0;

  next[datalen] = '\0';

  while (status==0 && next && (subreq=get_sub_req(next,&reqlen))) {

    /* process a whole sub request */
    status = pay_svr_do_post(pconn,subreq,reqlen);

    pay_svr_print_debug_req(subreq,reqlen,status);


    if (status==-1) 
      pay_svr_do_error(pconn);

    pconn->l5opt.tx(net,pconn);

    next += reqlen ;
  }

  /* REMEMBER TO update the read pointer of rx buffer */
  dbuffer_lseek(pconn->rxb,datalen,SEEK_CUR,0);

  return 0;
}

int pay_svr_tx(Network_t net, connection_t pconn)
{
  pconn->l4opt.tx(net,pconn);

  return 0;
}

int pay_svr_pre_init(int argc, char *argv[])
{
  if (parse_cmd_line(argc,argv))
    return 1;

  if (init_config(&g_paySvrConf.m_conf,g_paySvrConf.conf_path))
    return -1;

  strcpy(g_paySvrConf.host,get_bind_address(&g_paySvrConf.m_conf));
  g_paySvrConf.port = get_listen_port(&g_paySvrConf.m_conf);
  g_paySvrConf.notify_port = get_notify_port(&g_paySvrConf.m_conf);

  g_paySvrConf.fd = new_tcp_svr(__net_atoi(g_paySvrConf.host),
                                g_paySvrConf.port);

  // the notify servlet
  g_paySvrConf.notify_fd = new_tcp_svr(__net_atoi(g_paySvrConf.host),
                                       g_paySvrConf.notify_port);

  return 0;
}

void pay_svr_release()
{
  close(g_paySvrConf.fd);

  delete_pay_action_entry(g_paySvrConf.m_pas0);
}

static
struct module_struct_s g_module = {

  .name = "pay server",

  .id = -1,

  .dyn_handle = NULL,

  .opts[inbound_l5] = {
    .rx = pay_svr_rx,
    .tx = pay_svr_tx,
    .init = pay_svr_init,
    .release = pay_svr_release,
  },
} ;


static 
void pay_svr_notify_release()
{
}

static 
int pay_svr_notify_rx(Network_t net, connection_t pconn)
{
  size_t datalen = dbuffer_data_size(pconn->rxb);
  dbuffer_t next = dbuffer_ptr(pconn->rxb,0);


  if (datalen==0L)
    return 0;

  pay_svr_do_error(pconn);
  pconn->l5opt.tx(net,pconn);

  log_debug("content: %s(%ld)\n",next,datalen);
  dbuffer_lseek(pconn->rxb,datalen,SEEK_CUR,0);

  return 0;
}

static 
int pay_svr_notify_tx(Network_t net, connection_t pconn)
{
  pconn->l4opt.tx(net,pconn);
  return 0;
}

static 
struct module_struct_s g_notify_module = {

  .name = "pay server notify",

  .id = -1,

  .dyn_handle = NULL,

  .opts[inbound_l5] = {
    .rx = pay_svr_notify_rx,
    .tx = pay_svr_notify_tx,
    .init = pay_svr_notify_init,
    .release = pay_svr_notify_release,
  },
};

static 
int pay_svr_notify_init(Network_t net)
{
  net->reg_local(net,g_paySvrConf.notify_fd,g_notify_module.id);

  log_info("done!\n");
  return 0;
}


static
int parse_cmd_line(int argc, char *argv[])
{
  //char *ptr = 0;

  for (int i=1; i<argc; i++) {
    if (!strcmp(argv[i],"-cp") && i+1<argc) {
      strcpy(g_paySvrConf.conf_path,argv[i+1]);
    }
    else if (!strcmp(argv[i],"-h")) {
      printf("module '%s' help message\n",THIS_MODULE->name);
      return 1;
    }
  }
  return 0;
}

static
int pay_svr_init(Network_t net)
{
  net->reg_local(net,g_paySvrConf.fd,THIS_MODULE->id);

  g_paySvrConf.m_pas0 = new_pay_action_entry();

  register_pay_action_list();

  log_info("done!\n");

  return 0;
}

static 
void pay_svr_dump_params()
{
  char *host = (char*)g_paySvrConf.host;
  int port = g_paySvrConf.port;


  log_info("module '%s' param list: =================\n",THIS_MODULE->name);
  log_info("bound to: %s:%d, notify port: %d\n",host,port,g_paySvrConf.notify_port);
  log_info("module param list end =================\n");
}

void pay_svr_module_init(int argc, char *argv[])
{
  if (pay_svr_pre_init(argc,argv))
    return ;

  pay_svr_dump_params();

  register_module(&g_module);
  register_module(&g_notify_module);
}

void pay_svr_module_exit()
{
  pay_svr_release();
}
