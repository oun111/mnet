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
#include "action.h"
#include "dbuffer.h"
#include "log.h"
#include "file_cache.h"
#include "instance.h"
#include "socket.h"
#include "module.h"
#include "tree_map.h"
#include "kernel.h"
#include "myrbtree.h"
#include "http_utils.h"
#include "jsons.h"



struct http_svr_conf {
  char host[32];

  int fd;
  int port;

  int notify_fd ;
  int notify_port ;

  http_action_entry_t m_act0 ;

} g_httpSvrConf = {
  .host = "127.0.0.1",
  .port = 4321,
  .fd = -1,
  .notify_fd = -1,

};


static struct http_svr_keywords_t {
  const char *channel ;

} g_httpSvrKeywords = {
  .channel = "channel",

};



static int http_svr_init(Network_t net);

static int http_svr_notify_init(Network_t net);


http_action_entry_t get_http_action_entry()
{
  return g_httpSvrConf.m_act0 ;
}

static 
int http_svr_do_error(connection_t pconn)
{
  const char *bodyPage = "{ \"status\":\"error\" }\r\n" ;

  create_http_simple_res(&pconn->txb,bodyPage);
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

static 
int process_param_list(Network_t net, connection_t pconn, 
                       char *kvlist, const char *action)
{
  int ret = -1;
  http_action_t pos ;
  char key[256] = "", *chan = 0;
  tree_map_t map = new_tree_map();


  uri_to_map(kvlist,strlen(kvlist),map);
  chan = get_tree_map_value(map,(char*)g_httpSvrKeywords.channel,
                               strlen(g_httpSvrKeywords.channel));
  if (!chan) {
    log_error("no '%s' param found\n",g_httpSvrKeywords.channel);
    goto __end;
  }

  // construct the 'key'
  snprintf(key,256,"%s/%s",chan,action);

  if ((pos=get_http_action(g_httpSvrConf.m_act0,key))) {
    ret = pos->cb(net,pconn,map);
  }

__end:
  delete_tree_map(map);

  return ret;
}

static 
int http_svr_do_get(Network_t net, connection_t pconn, 
                   const dbuffer_t req, const size_t sz_in)
{
  char action[256] = "";
  size_t ln = 0L ;
  char body[256] = "";


  /* 
   * parse action 
   */
  ln = sizeof(action);
  if (get_http_hdr_field_str(req,sz_in,"/","?",action,&ln)==-1) {
    log_error("get action fail!");
    return -1;
  }

  log_debug("action: %s\n",action);

  /*
   * parse param list
   */
  ln = sizeof(body);
  if (get_http_hdr_field_str(req,sz_in,"?"," ",body,&ln)==-1) {
    log_error("get body fail!");
    return -1;
  }

  log_debug("values: %s\n",body);

  return process_param_list(net,pconn,body,action);
}

static 
int http_svr_do_post(Network_t net, connection_t pconn, 
                    const dbuffer_t req, const size_t sz_in)
{
  char action[256] = "";
  size_t ln = 256, szhdr = get_http_hdr_size(req,sz_in) ;
  char *pbody = get_http_body_ptr(req,sz_in);


  /* 
   * parse action 
   */
  if (get_http_hdr_field_str(req,sz_in,"/"," ",action,&ln)==-1) {
    log_error("get action fail!");
    return -1;
  }

  log_debug("action: %s\n",action);

  /*
   * parse param list
   */
  if (sz_in == szhdr) {
    log_error("no param list\n");
    http_svr_do_error(pconn);
    return 0;
  }

  log_debug("values: %s\n",pbody);

  return process_param_list(net,pconn,pbody,action);
}

ssize_t http_svr_rx_raw(Network_t net, connection_t pconn)
{
  size_t sz_in = dbuffer_data_size(pconn->rxb);
  dbuffer_t b = dbuffer_ptr(pconn->rxb,0);
  ssize_t szReq = 0L, szhdr= 0L;

  /* mysql packet has header length==4 */
  if (sz_in==0) {
    return 0L;
  }

  szhdr = get_http_hdr_size(b,sz_in);
  // not enough data
  if (szhdr==0L) {
    //log_error("no header size\n");
    return -1;
  }

  szReq = get_http_body_size(b,sz_in);
  // no 'content-length'?
  if (szReq==0L) {
    //log_info("no body size\n");
  }

  // total req size
  szReq += szhdr ;

  return szReq==sz_in?szReq:-1;
}

static
int http_svr_rx(Network_t net, connection_t pconn)
{
  size_t sz_in = 0L;
  int rc = 0;

  while ((sz_in=http_svr_rx_raw(net,pconn))>0) {
    dbuffer_t inb = dbuffer_ptr(pconn->rxb,0);

    inb[sz_in] = '\0';

    log_debug("%s...(size %zu) - %s\n",inb,sz_in,!rc?"ok":"fail");

    /* process a whole sub request */
    if (strstr(inb,"POST")) {
      rc = http_svr_do_post(net,pconn,inb,sz_in);
    }
    else {
      rc = http_svr_do_get(net,pconn,inb,sz_in);
    }

    if (rc==-1) 
      http_svr_do_error(pconn);

    pconn->l5opt.tx(net,pconn);

    /* REMEMBER TO update the read pointer of rx buffer */
    dbuffer_lseek(pconn->rxb,sz_in,SEEK_CUR,0);
  }

  return 0;
}

static
int http_svr_tx(Network_t net, connection_t pconn)
{
  pconn->l4opt.tx(net,pconn);

  return 0;
}

static int http_svr_pre_init()
{
  g_httpSvrConf.m_act0 = new_http_action_entry();

  if (g_httpSvrConf.port>0) {
    g_httpSvrConf.fd = new_tcp_svr(__net_atoi(g_httpSvrConf.host),g_httpSvrConf.port);
  }

  if (g_httpSvrConf.notify_port>0) {
    g_httpSvrConf.notify_fd = new_tcp_svr(__net_atoi(g_httpSvrConf.host),g_httpSvrConf.notify_port);
  }

  return 0;
}

static
void http_svr_save_host_info(const char *host, int listen_port,
                             int notify_port)
{
  g_httpSvrConf.port        = listen_port ;

  g_httpSvrConf.notify_port = notify_port ;

  strncpy(g_httpSvrConf.host,host,sizeof(g_httpSvrConf.host));
}

static
void http_svr_release()
{
  close(g_httpSvrConf.fd);

  delete_http_action_entry(g_httpSvrConf.m_act0);
}

static
struct module_struct_s g_module = {

  .name = "http server",

  .id = -1,

  .dyn_handle = NULL,

  .ssl = false,

  .opts[inbound_l5] = {
    .rx = http_svr_rx,
    .tx = http_svr_tx,
    .init = http_svr_init,
    .release = http_svr_release,
  },
} ;


static 
void http_svr_notify_release()
{
}

static 
int http_svr_notify_rx(Network_t net, connection_t pconn)
{
  size_t datalen = dbuffer_data_size(pconn->rxb);
  dbuffer_t next = dbuffer_ptr(pconn->rxb,0);


  if (datalen==0L)
    return 0;

  http_svr_do_error(pconn);
  pconn->l5opt.tx(net,pconn);

  log_debug("content: %s(%ld)\n",next,datalen);
  dbuffer_lseek(pconn->rxb,datalen,SEEK_CUR,0);

  return 0;
}

static 
int http_svr_notify_tx(Network_t net, connection_t pconn)
{
  pconn->l4opt.tx(net,pconn);
  return 0;
}

static 
struct module_struct_s g_notify_module = {

  .name = "http server notify",

  .id = -1,

  .dyn_handle = NULL,

  .ssl = false,

  .opts[inbound_l5] = {
    .rx = http_svr_notify_rx,
    .tx = http_svr_notify_tx,
    .init = http_svr_notify_init,
    .release = http_svr_notify_release,
  },
};

static 
int http_svr_notify_init(Network_t net)
{
  net->reg_local(net,g_httpSvrConf.notify_fd,g_notify_module.id);

  log_info("done!\n");
  return 0;
}


static
int http_svr_init(Network_t net)
{
  net->reg_local(net,g_httpSvrConf.fd,THIS_MODULE->id);

  log_info("done!\n");

  return 0;
}

static 
void http_svr_dump_params()
{
  char *host = (char*)g_httpSvrConf.host;
  int port = g_httpSvrConf.port;


  log_info("module '%s' param list: =================\n",THIS_MODULE->name);
  log_info("bound to: %s:%d, notify port: %d\n",host,port,g_httpSvrConf.notify_port);
  log_info("module param list end =================\n");
}

void __http_svr_entry(const char *host, int port, int notify_port)
{
  http_svr_save_host_info(host,port,notify_port);

  if (http_svr_pre_init())
    return ;

  http_svr_dump_params();

  register_module(&g_module);
  register_module(&g_notify_module);

#if 0
  {
    extern void test_crypto();
    test_crypto();
  }
#endif
}

void __http_svr_exit()
{
  http_svr_release();
}

