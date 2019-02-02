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
#include "extra_modules.h"
#include "http_utils.h"
#include "pay_data.h"
#include "jsons.h"
#include "backend.h"




struct pay_svr_conf {
  char host[32];

  int fd;
  int port;

  int notify_fd ;
  int notify_port ;

  char conf_path[PATH_MAX];

  struct paySvr_config_s m_conf;

  pay_action_entry_t m_pas0 ;

  pay_channels_entry_t m_paych ;

  struct backend_entry_s m_backends;

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


int init_pay_data()
{
  tm_item_t pos,n;
  tm_item_t pos1,n1;
  tree_map_t entry = g_paySvrConf.m_conf.chan_cfg;


  g_paySvrConf.m_paych = new_pay_channels_entry();

  rbtree_postorder_for_each_entry_safe(pos,n,&entry->u.root,node) {
    tree_map_t chansub = pos->nest_map ;

    if (!chansub)
      continue ;

    rbtree_postorder_for_each_entry_safe(pos1,n1,&chansub->u.root,node) {
      add_pay_data(g_paySvrConf.m_paych,pos->key,pos1->key,pos1->nest_map);
    }
  }

  return 0;
}

pay_action_entry_t get_pay_action_entry()
{
  return g_paySvrConf.m_pas0 ;
}

backend_entry_t get_backend_entry()
{
  return &g_paySvrConf.m_backends;
}

static 
int pay_svr_do_error(connection_t pconn)
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
  pay_action_t pos ;
  char key[256] = "", *payChan = 0;
  tree_map_t map = new_tree_map();


  uri_to_map(kvlist,strlen(kvlist),map);
  payChan = get_tree_map_value(map,(char*)g_paySvrKeywords.channel,
                               strlen(g_paySvrKeywords.channel));
  if (!payChan) {
    log_error("no '%s' param found\n",g_paySvrKeywords.channel);
    goto __end;
  }

  // pay route
  pay_data_t pdt = get_pay_route(g_paySvrConf.m_paych,payChan);

  if (!pdt) {
    log_error("no pay route to channel '%s'\n",payChan);
    goto __end;
  }

  log_info("route to item '%s'\n",pdt->subname);

  // construct the 'key'
  snprintf(key,256,"%s/%s",payChan,action);

  if ((pos=get_pay_action(g_paySvrConf.m_pas0,key))) {
    ret = pos->cb(net,pconn,pdt,map);
  }

__end:
  delete_tree_map(map);

  return ret;
}

static 
int pay_svr_do_get(Network_t net, connection_t pconn, 
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
int pay_svr_do_post(Network_t net, connection_t pconn, 
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
    pay_svr_do_error(pconn);
    return 0;
  }

  log_debug("values: %s\n",pbody);

  return process_param_list(net,pconn,pbody,action);
}

ssize_t pay_svr_http_rx(Network_t net, connection_t pconn)
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
int pay_svr_rx(Network_t net, connection_t pconn)
{
  size_t sz_in = 0L;
  int rc = 0;

  while ((sz_in=pay_svr_http_rx(net,pconn))>0) {
    dbuffer_t inb = dbuffer_ptr(pconn->rxb,0);

    inb[sz_in] = '\0';

    log_debug("%s...(size %zu) - %s\n",inb,sz_in,!rc?"ok":"fail");

    /* process a whole sub request */
    if (strstr(inb,"POST")) {
      rc = pay_svr_do_post(net,pconn,inb,sz_in);
    }
    else {
      rc = pay_svr_do_get(net,pconn,inb,sz_in);
    }

    if (rc==-1) 
      pay_svr_do_error(pconn);

    pconn->l5opt.tx(net,pconn);

    /* REMEMBER TO update the read pointer of rx buffer */
    dbuffer_lseek(pconn->rxb,sz_in,SEEK_CUR,0);
  }

  return 0;
}

static
int pay_svr_tx(Network_t net, connection_t pconn)
{
  pconn->l4opt.tx(net,pconn);

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
      printf("help message\n");
      return 1;
    }
  }
  return 0;
}

int pay_svr_pre_init(int argc, char *argv[])
{
  if (parse_cmd_line(argc,argv))
    return 1;

  if (init_config(&g_paySvrConf.m_conf,g_paySvrConf.conf_path))
    return -1;

  init_backend_entry(&g_paySvrConf.m_backends);

  g_paySvrConf.m_pas0 = new_pay_action_entry();

  init_pay_data();

  strcpy(g_paySvrConf.host,get_bind_address(&g_paySvrConf.m_conf));

  g_paySvrConf.notify_port = get_notify_port(&g_paySvrConf.m_conf);
  g_paySvrConf.port        = get_listen_port(&g_paySvrConf.m_conf);

  g_paySvrConf.fd          = new_tcp_svr(__net_atoi(g_paySvrConf.host),g_paySvrConf.port);

  g_paySvrConf.notify_fd   = new_tcp_svr(__net_atoi(g_paySvrConf.host),g_paySvrConf.notify_port);

  return 0;
}

static
void pay_svr_release()
{
  close(g_paySvrConf.fd);

  delete_pay_channels_entry(g_paySvrConf.m_paych);

  delete_pay_action_entry(g_paySvrConf.m_pas0);

  release_all_backends(&g_paySvrConf.m_backends);

  free_config(&g_paySvrConf.m_conf);
}

static
struct module_struct_s g_module = {

  .name = "pay server",

  .id = -1,

  .dyn_handle = NULL,

  //.ssl = false,

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

  //.ssl = false,

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
int pay_svr_init(Network_t net)
{
  net->reg_local(net,g_paySvrConf.fd,THIS_MODULE->id);

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
  register_extra_modules();
}

void pay_svr_module_exit()
{
  pay_svr_release();
}
