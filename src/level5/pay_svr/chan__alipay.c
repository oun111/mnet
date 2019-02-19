
#include <time.h>
#include <string.h>
#include "action.h"
#include "log.h"
#include "instance.h"
#include "module.h"
#include "socket.h"
#include "ssl.h"
#include "config.h"
#include "http_svr.h"
#include "http_utils.h"
#include "backend.h"
#include "order.h"
#include "global.h"
#include "crypto.h"
#include "base64.h"
#include "auto_id.h"


#define ALIPAY_DBG  0


struct alipay_data_s {

  struct auto_id_s aid ;

} g_alipayData  ;


static int alipay_init(Network_t net);

static struct http_action_s action__alipay_order;


static 
int do_ok(connection_t pconn)
{
  const char *bodyPage = "{\"status\":\"success\"}\r\n" ;

  create_http_normal_res(&pconn->txb,pt_json,bodyPage);

  return 0;
}

static
int alipay_tx(Network_t net, connection_t pconn)
{
  pconn->l4opt.tx(net,pconn);
  return 0;
}

static
int alipay_rx(Network_t net, connection_t pconn)
{
  ssize_t sz_in = 0L;
  dbuffer_t b = NULL;


#if 0
  if (ps && ps->state!=s_ok) {
    return 0;
  }
#endif

  if ((sz_in=http_svr_rx_raw(net,pconn))>0) {

    backend_entry_t bentry = get_backend_entry();
    backend_t be = get_backend(bentry,pconn->fd);
    connection_t peer = be->peer ;
    pay_data_t pd = be->data ;

    b = dbuffer_ptr(pconn->rxb,0);
    b[sz_in] = '\0' ;

    // XXX: debug
    printf("%s",b);

    dbuffer_lseek(pconn->rxb,sz_in,SEEK_CUR,0);

    // reply to client
    if (peer) {
      do_ok(peer);
      peer->l4opt.tx(net,peer);
    }

    // accumulate weight
    pd->weight ++ ;
  }

  return 0;
}

static
void alipay_release()
{
}

struct module_struct_s g_alipay_mod = {

  .name = "pay channel alipay",

  .id = -1,

  .dyn_handle = NULL,

  .ssl = true,

  .opts[outbound_l5] = {
    .rx = alipay_rx,
    .tx = alipay_tx,
    .init = alipay_init,
    .release = alipay_release,
  },
};


static
int deal_crypto(tree_map_t pay_params)
{
  tree_map_t crypto_map = get_tree_map_nest(pay_params,CRYPTO);
  tree_map_t pay_data = get_tree_map_nest(pay_params,PAY_DATA);
  char *privkeypath = 0;
  dbuffer_t sign_params = 0; 
  unsigned char *sign = 0;
  unsigned int sz_out = 0;
  unsigned char *final_res = 0;
  int sz_res = 0;
  int ret = 0;


  if (!crypto_map) {
    return 1;
  }

  // reset 'sign' field
  put_tree_map_string(pay_data,"sign",(char*)"");

  sign_params = create_html_params(pay_data);
  //log_debug("sign string: %s, size: %zu\n",sign_params,strlen(sign_params));

  privkeypath = get_tree_map_value(crypto_map,PRIVKEY);
  if (rsa_private_sign(privkeypath,sign_params,&sign,&sz_out)<0) {
    ret = -1;
    goto __done;
  }

  sz_res = sz_out<<2 ;
  final_res = alloca(sz_res);

  if (base64_encode(sign,sz_out,final_res,&sz_res)<0) {
    log_error("base64 error\n");
    ret = -1;
    goto __done;
  }

  put_tree_map_string(pay_data,"sign",(char*)final_res);

__done:
  drop_dbuffer(sign_params);

  if (sign)
    free(sign);
  return ret;
}

static 
int update_alipay_biz(tree_map_t user_params, tree_map_t pay_params)
{
  tree_map_t pay_data = get_tree_map_nest(pay_params,PAY_DATA);
  tree_map_t pay_biz = get_tree_map_nest(pay_params,"pay_biz");
  time_t curr = time(NULL);
  struct tm *tm = localtime(&curr);
  char tb[96] = "";
  char *body = 0, *subject = 0, *out_trade_no = 0, *amount=0;


  if (!pay_biz) {
    log_error("no 'pay_biz' found\n");
    return -1;
  }

  body = get_tree_map_value(user_params,"body");
  if (!body) {
    log_error("no 'body' given\n");
    return -1;
  }

  subject = get_tree_map_value(user_params,"subject");
  if (!body) {
    log_error("no 'subject' given\n");
    return -1;
  }

#if 0
  out_trade_no = get_tree_map_value(user_params,"out_trade_no");
  if (!body) {
    log_error("no 'out_trade_no' given\n");
    return -1;
  }
#else
  out_trade_no = aid_add_and_fetch(&g_alipayData.aid,1L);
#endif

  amount = get_tree_map_value(user_params,"total_amount");
  if (!body) {
    log_error("no 'amount' given\n");
    return -1;
  }

  snprintf(tb,96,"%d-%d-%d %d:%d:%d",tm->tm_year+1900,
           tm->tm_mon+1,tm->tm_mday,tm->tm_hour,tm->tm_min,
           tm->tm_sec);
  put_tree_map_string(pay_data,"timestamp",tb);

  put_tree_map_string(pay_biz,"body",body);
  put_tree_map_string(pay_biz,"subject",subject);
  put_tree_map_string(pay_biz,"out_trade_no",out_trade_no);

  // $ express an integer
  snprintf(tb,96,"$%s",amount);
  put_tree_map_string(pay_biz,"total_amount",tb);


  jsonKV_t *pr = jsons_parse_tree_map(pay_biz);
  dbuffer_t strBiz = alloc_default_dbuffer();

  jsons_toString(pr,&strBiz);
  put_tree_map_string(pay_data,"biz_content",strBiz);

  jsons_release(pr);
  drop_dbuffer(strBiz);

  // deals with cryptographic
  if (deal_crypto(pay_params)<0) {
    return -1;
  }

  return 0;
}

//static 
connection_t 
do_out_connect(Network_t net, connection_t peer_conn, 
               const char *url, void *data)
{
  bool is_ssl = true ;
  int port = -1, fd = 0 ;
  char host[128] = "";
  unsigned long addr = 0L;
  connection_t out_conn = NULL;


  parse_http_url(url,host,128,&port,NULL,0L,&is_ssl);
  //printf("url: %s, isssl: %d, host: %s, port: %d\n",url,is_ssl,host,port);

  // just 'http' connection
  if (!(is_ssl || port==443)) {

    if (port==-1) 
      port = 80 ;

    g_alipay_mod.ssl = false ;
  }
  else {
    if (port==-1)
      port = 443 ;
  }

  addr = hostname_to_uladdr(host) ;

  // connect to server 
  fd = new_tcp_client(addr,port);

  if (fd<0) {
    return NULL;
  }

  out_conn = net->reg_outbound(net,fd,g_alipay_mod.id);

  // save backend info
  create_backend(get_backend_entry(),fd,peer_conn,data);

  return out_conn;
}

static
int create_alipay_link(connection_t out_conn, const char *url, tree_map_t pay_data)
{
  dbuffer_t wholeUrl = alloc_default_dbuffer();
  dbuffer_t strParams = create_html_params(pay_data);
  size_t sz_res = 0L;
  dbuffer_t resbody = 0;


  write_dbuf_str(wholeUrl,url);
  append_dbuf_str(wholeUrl,"?");
  append_dbuf_str(wholeUrl,strParams);

  sz_res = strlen(wholeUrl)+strlen(strParams)+20;
  resbody = alloc_dbuffer(sz_res);

  snprintf(resbody,sz_res,"{\"location\":\"%s\",\"status\":0}",wholeUrl);

  create_http_normal_res(&out_conn->txb,pt_json,resbody);

  drop_dbuffer(wholeUrl);
  drop_dbuffer(strParams);
  drop_dbuffer(resbody);
  
  return 0;
}

static 
int create_order(tree_map_t pay_params, tree_map_t user_params)
{
  int ret = 0;
  char odrid[ODR_ID_SIZE] = "";
  order_entry_t pe = get_order_entry();
  char *pOdrId = aid_add_and_fetch(&g_alipayData.aid,0L);
  char *mch_no = get_tree_map_value(user_params,"mch_id");
  char *nurl = get_tree_map_value(user_params,"notify_url");
  char *tno = get_tree_map_value(user_params,"out_trade_no");
  char *amt = get_tree_map_value(user_params,"total_amount");
  char *chan = action__alipay_order.channel;
  tree_map_t pay_data = get_tree_map_nest(pay_params,PAY_DATA);
  char *chan_mch_no = NULL;

  if (!mch_no) {
    log_error("no merchant id supplied!\n");
    return -1;
  }

  if (!nurl) {
    log_error("no notify url supplied!\n");
    return -1;
  }

  if (!tno) {
    log_error("no out trade number supplied!\n");
    return -1;
  }

  if (!amt) {
    log_error("no amount supplied!\n");
    return -1;
  }

  if (!pay_data) {
    log_error("no pay data in config!\n");
    return -1;
  }

  chan_mch_no = get_tree_map_value(pay_data,"app_id");

  ret = save_order(pe,pOdrId,mch_no,nurl,tno,chan,chan_mch_no,atof(amt));
  if (ret) {
    log_error("save new order(id: %s) fail!\n",odrid);
    return -1;
  }

  return 0;
}

static 
int do_alipay_order(Network_t net,connection_t pconn,tree_map_t user_params)
{
  char *url = 0;
  connection_t out_conn = pconn ;
  tree_map_t pay_data  = NULL ;
  const char *payChan = action__alipay_order.channel ;
  pay_data_t pd = get_pay_route(get_pay_channels_entry(),payChan);
  tree_map_t pay_params = NULL ;


  if (!pd) {
    log_error("no pay data for channel '%s'\n",payChan);
    return -1;
  }

  pay_params = pd->pay_params ;
  url = get_tree_map_value(pay_params,REQ_URL);

  if (!url) {
    log_error("no 'url' configs\n");
    return -1;
  }

#if 0
  // connect to remote host if needed
  out_conn = do_out_connect(net,pconn,url,pd);
  if (!out_conn) {
    log_error("connect to '%s' fail\n",url);
    return -1;
  }
#endif

  //
  update_alipay_biz(user_params,pay_params);

  pay_data = get_tree_map_nest(pay_params,PAY_DATA);

#if ALIPAY_DBG==1
  int param_type = 0;
  char *str = get_tree_map_value(pay_params,PARAM_TYPE);

  if (str) {
    param_type = !strcmp(str,"html")?pt_html:pt_json;
  }

  if (create_browser_redirect_req(&out_conn->txb,url,param_type,pay_data)) {
    return -1;
  }
#else
  if (create_alipay_link(out_conn,url,pay_data)) {
    return -1;
  }
#endif

  if (create_order(pay_params,user_params)) {
    return -1;
  }

  if (!out_conn->ssl || out_conn->ssl->state==s_ok) {
    alipay_tx(net,out_conn);
  }

  return 0;
}


static 
int do_alipay_notify(Network_t net,connection_t pconn,tree_map_t user_params)
{
  const char *alipay_notify_res = "success";

  create_http_normal_res(&pconn->txb,pt_json,alipay_notify_res);

  dump_tree_map(user_params);

  /**
   * TODO: 
   * 1. connect merchant server(treat as 'backend')
   * 2. post notify to merchant
   */

  return 0;
}

static
struct http_action_s action__alipay_order = 
{
  .key = "alipay/payApi/pay",
  .channel = "alipay",
  .action  = "payApi/pay",
  .cb      = do_alipay_order,
} ;

static
struct http_action_s action__alipay_notify = 
{
  .key = "alipay/notify",
  .channel = "alipay",
  .action  = "notify",
  .cb      = do_alipay_notify,
} ;


static
int alipay_init(Network_t net)
{
  http_action_entry_t pe = get_http_action_entry();
  

  aid_reset(&g_alipayData.aid,"alp_id");

  add_http_action(pe,&action__alipay_order);
  add_http_action(pe,&action__alipay_notify);

  return 0;
}

