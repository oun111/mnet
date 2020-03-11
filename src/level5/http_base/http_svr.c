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



#define INTERNAL_ERR(__errbuf__,fmt,arg...) do{\
  char msg[64],tmp[96]; \
  snprintf(msg,sizeof(msg),"%s","request packet error!"); \
  if (__errbuf__) create_http_normal_res((__errbuf__),500,pt_html,msg); \
  snprintf(tmp,sizeof(tmp),fmt,##arg);\
  log_error("%s\n",tmp);  \
} while(0)


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



static int http_svr_init(Network_t net);


http_action_entry_t get_http_action_entry()
{
  return g_httpSvrConf.m_act0 ;
}

#if 0
static 
int http_svr_do_error(connection_t pconn)
{
  const char *bodyPage = "{ \"status\":\"error\" }\r\n" ;

  create_http_normal_res(&pconn->txb,pt_json,bodyPage);
  return 0;
}
#endif

static 
int process_param_list(Network_t net, connection_t pconn, 
                       char *kvlist, const char *action)
{
  int ret = -1;
  http_action_t pos ;
  char *key = (char*)action ;
  tree_map_t map = new_tree_map();


  uri_to_map(kvlist,strlen(kvlist),map);

  if ((pos=get_http_action(g_httpSvrConf.m_act0,key))) {
    ret = pos->cb(net,pconn,map);
  }
  else {
    INTERNAL_ERR(&pconn->txb,"found no action '%s'",key);
  }

  delete_tree_map(map);

  return ret;
}

static 
int http_svr_do_get(Network_t net, connection_t pconn, 
                   const dbuffer_t req, const size_t sz_in)
{
  char action[256] = "";
  size_t ln = 0L ;
  char body[/*256*/4096] = "";


  /* 
   * parse action 
   */
  ln = sizeof(action);
  if (get_http_hdr_field_str(req,sz_in,"/","?",action,&ln)==-1) {

    if (get_http_hdr_field_str(req,sz_in,"/"," ",action,&ln)==-1) {
      char *pa = action ;
      INTERNAL_ERR(&pconn->txb,"action '%s' format error",/*action*/pa);
      return -1;
    }
  }

  //log_debug("action: %s\n",action);

  /*
   * parse param list
   */
  ln = sizeof(body);
  if (get_http_hdr_field_str(req,sz_in,"?"," ",body,&ln)==-1) {
    log_info("found no body\n");
    //return -1;
  }

  //log_debug("values: %s\n",body);

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
    char *pa = action ;
    INTERNAL_ERR(&pconn->txb,"action '%s' format error",/*action*/pa);
    return -1;
  }

  log_debug("action: %s\n",action);

  /*
   * parse param list
   */
  if (sz_in == szhdr) {
    INTERNAL_ERR(&pconn->txb,"no param list");
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
    log_error("no header size(sz_in: %zu)\n",sz_in);
    log_error("rx: %s\n",b);
    // invalid http packet!!
    return unlikely(sz_in>4096)?-2:-1;
  }

  szReq = get_http_body_size(b,sz_in);
  // no 'content-length'?
  if (szReq==0L) {
    //log_info("no body size\n");
  }

  log_debug("szreq: %zd, sz_in: %zu, szhdr: %zd\n",szReq,sz_in,szhdr);

  if ((szReq+szhdr)>sz_in) {
    log_debug("incomplete req\n");
    return -1;
  }

  // total req size
  szReq += szhdr ;

  //return szReq==sz_in?szReq:-1;
  return szReq;
}

static
int http_svr_rx(Network_t net, connection_t pconn)
{
  ssize_t sz_in = 0L;
  int rc = 0;

  while ((sz_in=http_svr_rx_raw(net,pconn))>0) {
    dbuffer_t inb = dbuffer_ptr(pconn->rxb,0);
    char ch = inb[sz_in];

    inb[sz_in] = '\0';

    log_debug("%s...(size %zu) - %s\n",inb,sz_in,!rc?"ok":"fail");

    /* process a whole sub request */
    if (strstr(inb,"POST")) {
      rc = http_svr_do_post(net,pconn,inb,sz_in);
    }
    else {
      rc = http_svr_do_get(net,pconn,inb,sz_in);
    }

    inb[sz_in] = ch;

    if (rc==-1 && dbuffer_data_size(pconn->txb)==0L) 
      INTERNAL_ERR(&pconn->txb,"internal error");

    pconn->l5opt.tx(net,pconn);

    /* REMEMBER TO update the read pointer of rx buffer */
    dbuffer_lseek(pconn->rxb,sz_in,SEEK_CUR,0);
  }

  if (unlikely(sz_in==-2)) {
    return -1;
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
int http_svr_init(Network_t net)
{
  net->reg_local(net,g_httpSvrConf.fd,THIS_MODULE->id);

  net->reg_local(net,g_httpSvrConf.notify_fd,THIS_MODULE->id);

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

#if 0
  {
    extern void test_crypto();
    test_crypto();
  }
#endif
}

void __http_svr_exit()
{
  //http_svr_release();
}

