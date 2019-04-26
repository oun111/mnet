
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
#include "rds_order.h"
#include "merchant.h"
#include "myredis.h"
#include "pay_svr.h"
#include "crypto.h"
#include "base64.h"
#include "auto_id.h"
#include "L4.h"
#include "url_coder.h"
#include "timer.h"

//#include "myrbtree.h"


#define ALIPAY_DBG  0



#define REQ_URL      "req_url"
#define PARAM_TYPE   "param_type"
#define PUBKEY       "public_key_path"
#define PRIVKEY      "private_key_path"
#define SIGNTYPE     "sign_type"
#define SIGN         "sign"
#define OTNO         "out_trade_no"
#define TNO          "trade_no"
#define MCHID        "mch_id"
#define TAMT         "total_amount"
#define RETURL       "return_url"
#define AMT          "amount"
#define NURL         "notify_url"
#define STATUS       "status"
#define APPID        "app_id"


struct alipay_data_s {

  struct auto_id_s aid ;

  struct rds_order_entry_s m_rOrders ;

  struct myredis_s m_rds ;

  // used by dynamic updater
  struct dynamic_updater_s {
    struct myredis_s m_rds ;
    dbuffer_t push_msg ;
  } du ;

} g_alipayData  ;


static int alipay_init(Network_t net);

static void alipay_release();

static struct http_action_s action__alipay_order;


static
int alipay_tx(Network_t net, connection_t pconn)
{
  return pconn->l4opt.tx(net,pconn);
}

static
int process_notify2user_resp(connection_t pconn, void *data, dbuffer_t resp, 
                             size_t sz)
{
  char *body = get_http_body_ptr(resp,sz);
  rds_order_entry_t pre = &g_alipayData.m_rOrders;
  paySvr_config_t pc = get_running_configs();
  mysql_conf_t mcfg = get_mysql_configs(pc);
  order_info_t po = data;
  int st=s_notify_fail;

  if (!body) {
    log_error("found no body!\n");
    return -1;
  }

  if (!strcasecmp(body,"success")) {
    log_debug("user receive notify ok!\n");
    st = s_notify_ok ;
  }

  // update order user notify status
  set_order_un_status(po,st);

  // update redis
  if (save_rds_order(pre,mcfg->order_table,po)) {
    log_error("update order '%s' to redis fail\n",po->id);
  }

  // TODO: if 'un status' is not s_notify_ok, then
  //  we should notify user for serial times later

  close(pconn->fd);

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


    if (!be) {
      log_error("no backend info by fd %d\n",pconn->fd);
      return -1;
    }

    // collect user raw http response
    b = dbuffer_ptr(pconn->rxb,0);
    b[sz_in] = '\0' ;

    log_debug("rx resp: %s",b);

    if (be->type==bt_notify2user) {
      process_notify2user_resp(pconn,be->data,b,sz_in);
    }

    dbuffer_lseek(pconn->rxb,sz_in,SEEK_CUR,0);

#if 0
    // reply to client
    if (be->peer) {
      do_ok(be->peer);
      be->peer->l4opt.tx(net,be->peer);
    }
#endif
  }

  return 0;
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
int do_signature(dbuffer_t *errbuf,tree_map_t pay_params,tree_map_t pay_data)
{
  char *privkeypath = 0;
  dbuffer_t sign_params = 0; 
  unsigned char *sign = 0;
  unsigned int sz_out = 0;
  unsigned char *final_res = 0;
  int sz_res = 0;
  int ret = -1;


  // reset 'sign' field
  put_tree_map_string(pay_data,SIGN,(char*)"");

  sign_params = create_html_params(pay_data);
  //log_debug("sign string: %s, size: %zu\n",sign_params,strlen(sign_params));

  privkeypath = get_tree_map_value(pay_params,PRIVKEY);
  if (rsa_private_sign(privkeypath,sign_params,&sign,&sz_out)!=1) {
    FORMAT_ERR(errbuf,"rsa sign error\n");
    goto __done;
  }

  sz_res = sz_out<<2 ;
  final_res = alloca(sz_res);

  if (base64_encode(sign,sz_out,final_res,&sz_res)<0) {
    FORMAT_ERR(errbuf,"base64 error\n");
    goto __done;
  }

  // save signature
  put_tree_map_string(pay_data,SIGN,(char*)final_res);

  urlencode_tree_map(pay_data);

  ret = 0;

__done:
  drop_dbuffer(sign_params);

  if (sign)
    free(sign);
  return ret;
}

static 
int update_alipay_biz(dbuffer_t *errbuf, tree_map_t user_params, 
                      tree_map_t pay_params, tree_map_t pay_data)
{
  tree_map_t pay_biz = NULL;
  time_t curr = time(NULL);
  struct tm *tm = localtime(&curr);
  char tmp[96] = "";
  char *body = 0, *subject = 0, *out_trade_no = 0, 
       *amount=0, *ret_url = 0, *orderid = 0;


  body = get_tree_map_value(user_params,"body");
  if (!body) {
    FORMAT_ERR(errbuf,"no 'body' found\n");
    return -1;
  }

  subject = get_tree_map_value(user_params,"subject");
  if (!subject) {
    FORMAT_ERR(errbuf,"no 'subject' found\n");
    return -1;
  }

  orderid = aid_add_and_fetch(&g_alipayData.aid,1L);
  log_debug("create new order id: %s\n",orderid);

  amount = get_tree_map_value(user_params,TAMT);
  if (!amount) {
    FORMAT_ERR(errbuf,"no 'amount' found\n");
    return -1;
  }

  ret_url = get_tree_map_value(user_params,RETURL);
  if (!ret_url) {
    FORMAT_ERR(errbuf,"no '%s' found\n",RETURL);
    return -1;
  }

  // user out_trade_no to be alipay request order id
  out_trade_no = get_tree_map_value(user_params,OTNO);
  if (!out_trade_no) {
    FORMAT_ERR(errbuf,"no 'out_trade_no' found\n");
    return -1;
  }


  pay_biz = new_tree_map();

  snprintf(tmp,sizeof(tmp),"%04d-%02d-%02d %02d:%02d:%02d",tm->tm_year+1900,
           tm->tm_mon+1,tm->tm_mday,tm->tm_hour,tm->tm_min,
           tm->tm_sec);
  put_tree_map_string(pay_data,"timestamp",tmp);

  put_tree_map_string(pay_data,APPID,
      get_tree_map_value(pay_params,APPID));

  put_tree_map_string(pay_data,"method",
      get_tree_map_value(pay_params,"method"));

  put_tree_map_string(pay_data,"format",
      get_tree_map_value(pay_params,"format"));

  put_tree_map_string(pay_data,"charset",
      get_tree_map_value(pay_params,"charset"));

  put_tree_map_string(pay_data,"version",
      get_tree_map_value(pay_params,"version"));

  put_tree_map_string(pay_data,NURL,
      get_tree_map_value(pay_params,NURL));

  put_tree_map_string(pay_data,"return_url",ret_url);

  put_tree_map_string(pay_data,SIGNTYPE,
      get_tree_map_value(pay_params,SIGNTYPE));

  put_tree_map_string(pay_biz,"body",body);
  put_tree_map_string(pay_biz,"subject",subject);
  put_tree_map_string(pay_biz,OTNO,out_trade_no);

  // $ express an integer
  snprintf(tmp,sizeof(tmp),"$%s",amount);
  put_tree_map_string(pay_biz,TAMT,tmp);

  put_tree_map_string(pay_biz,"product_code",
      get_tree_map_value(pay_params,"product_code"));

  put_tree_map_string(pay_biz,"timeout_express",
      get_tree_map_value(pay_params,"timeout_express"));

#if 1
  dbuffer_t strBiz = create_json_params(pay_biz);
#else
  dbuffer_t strBiz = alloc_default_dbuffer();
  {
    jsonKV_t *pr = jsons_parse_tree_map(pay_biz);
    jsons_toString(pr,&strBiz,/*true*/false);
    jsons_release(pr);
  }
#endif

  put_tree_map_string(pay_data,"biz_content",strBiz);


  drop_dbuffer(strBiz);
  delete_tree_map(pay_biz);

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

    // fixup
    is_ssl = false ;
  }
  else {
    if (port==-1)
      port = 443 ;

    // fixup
    is_ssl = true ;
  }

  // use ssl or not
  update_module_l4_opts(&g_alipay_mod,is_ssl);

  addr = hostname_to_uladdr(host) ;

  // connect to server 
  fd = new_tcp_client(addr,port);

  if (fd<0) {
    return NULL;
  }

  out_conn = net->reg_outbound(net,fd,g_alipay_mod.id);

  // save backend info
  create_backend(get_backend_entry(),fd,peer_conn,bt_notify2user,
                 data);

  return out_conn;
}

static
int create_alipay_link(dbuffer_t *errbuf,connection_t out_conn, const char *url, 
                       merchant_info_t pm, tree_map_t pay_data)
{
  dbuffer_t wholeUrl = alloc_default_dbuffer();
  dbuffer_t strParams = create_html_params(pay_data);
  tree_map_t res_map = new_tree_map();
  order_entry_t pe = get_order_entry();
  char *odrid = aid_fetch(&g_alipayData.aid);
  order_info_t po = get_order(pe,odrid);
  char tmp[16]="", *ptr = 0;
  int param_type = pt_json;


  if (!po) {
    FORMAT_ERR(errbuf,"found no order info by id %s\n",odrid);
    return -1;
  }

  ptr = get_tree_map_value(pm->mch_info,PARAM_TYPE);
  if (ptr) {
    param_type = !strcmp(ptr,"json")?pt_json:pt_html ;
  }
  
  write_dbuf_str(wholeUrl,url);
  append_dbuf_str(wholeUrl,"?");
  append_dbuf_str(wholeUrl,strParams);

  set_order_status(po,s_paying);

  put_tree_map_string(res_map,"location",wholeUrl);
  put_tree_map_string(res_map,STATUS,get_pay_status_str(po->status));
  put_tree_map_string(res_map,OTNO,po->mch.out_trade_no);
  put_tree_map_string(res_map,TNO,po->id);
  put_tree_map_string(res_map,MCHID,pm->id);

  snprintf(tmp,sizeof(tmp),"%s%.2f",param_type==pt_json?"$":"",po->amount);
  put_tree_map_string(res_map,AMT,tmp);

  create_http_normal_res2(&out_conn->txb,param_type,res_map);

  drop_dbuffer(wholeUrl);
  drop_dbuffer(strParams);

  delete_tree_map(res_map);
  
  return 0;
}

static 
int create_order(dbuffer_t *errbuf,tree_map_t pay_params, tree_map_t user_params)
{
  order_entry_t pe = get_order_entry();
  rds_order_entry_t pre = &g_alipayData.m_rOrders;
  char *pOdrId = aid_fetch(&g_alipayData.aid);
  char *mch_no = get_tree_map_value(user_params,MCHID);
  char *nurl = get_tree_map_value(user_params,NURL);
  char *tno = get_tree_map_value(user_params,OTNO);
  char *amt = get_tree_map_value(user_params,TAMT);
  char *chan = action__alipay_order.channel;
  char *chan_mch_no = NULL;
  paySvr_config_t pc = get_running_configs();
  mysql_conf_t mcfg = get_mysql_configs(pc);
  order_info_t po = 0;


  if (!mch_no) {
    FORMAT_ERR(errbuf,"no merchant id supplied!\n");
    return -1;
  }

  if (!nurl) {
    FORMAT_ERR(errbuf,"no notify url supplied!\n");
    return -1;
  }

  if (!tno) {
    FORMAT_ERR(errbuf,"no out trade number supplied!\n");
    return -1;
  }

  if (!amt) {
    FORMAT_ERR(errbuf,"no amount supplied!\n");
    return -1;
  }

  chan_mch_no = get_tree_map_value(pay_params,APPID);

  // check existence of order id both on 
  //  local cache and redis
  if (get_order(pe,pOdrId) ||
      is_rds_order_exist(pre,mcfg->order_table,pOdrId)) {
    FORMAT_ERR(errbuf,"order id '%s' duplicates!\n",pOdrId);
    return -1;
  }

  // check existence of merchant out-trade-no both on 
  //  local cache and redis
  if (get_order_by_outTradeNo(pe,tno) || 
      is_rds_outTradeNo_exist(pre,mcfg->order_table,tno)) {
    FORMAT_ERR(errbuf,"merchant out trade no '%s' duplicates!\n",tno);
    return -1;
  }

  // save order locally and on redis
  po = save_order(pe,pOdrId,mch_no,nurl,tno,chan,chan_mch_no,atof(amt));
  if (!po || save_rds_order(pre,mcfg->order_table,po)) {
    FORMAT_ERR(errbuf,"save order '%s' fail!\n",pOdrId);
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
  dbuffer_t reason = 0;
  pay_data_t pd = 0;
  char *tno = get_tree_map_value(user_params,OTNO);
  tree_map_t pay_params = NULL ;
  dbuffer_t *errbuf = &pconn->txb;
  merchant_entry_t pme = get_merchant_entry();
  merchant_info_t pm = 0;
  char *mch_id = NULL;
  int ret = -1;


  log_debug("requesting user out trade no: '%s'\n",tno);

  // TODO: verify merchant signature!!

  mch_id = get_tree_map_value(user_params,MCHID);
  if (!mch_id || !(pm=get_merchant(pme,mch_id))) {
    FORMAT_ERR(errbuf,"no such merchant '%s'\n",mch_id);
    goto __done ;
  }

  reason = alloc_default_dbuffer();
  pd = get_pay_route(get_pay_channels_entry(),payChan,&reason);
  if (!pd) {
    FORMAT_ERR(errbuf,"no pay route for channel '%s', reason: %s\n",
               payChan,reason);
    goto __done ;
  }

  log_debug("route to app_id: %s\n",get_tree_map_value(pd->pay_params,APPID));

  pay_params = pd->pay_params ;
  url = get_tree_map_value(pay_params,REQ_URL);

  if (!url) {
    FORMAT_ERR(errbuf,"no 'url' configs\n");
    goto __done ;
  }

  pay_data = new_tree_map();

  if (update_alipay_biz(&pconn->txb,user_params,pay_params,pay_data)) {
    goto __done ;
  }

  if (create_order(&pconn->txb,pay_params,user_params)) {
    goto __done ;
  }

  // deals with cryptographic
  if (do_signature(&pconn->txb,pay_params,pay_data)<0) {
    goto __done ;
  }

#if ALIPAY_DBG==1
  int param_type = 0;
  char *str = get_tree_map_value(pay_params,PARAM_TYPE);

  if (str) {
    param_type = !strcmp(str,"html")?pt_html:pt_json;
  }

  if (create_browser_redirect_req(&out_conn->txb,url,param_type,pay_data)) {
    goto __done ;
  }
#else
  if (create_alipay_link(&pconn->txb,out_conn,url,pm,pay_data)) {
    goto __done ;
  }
#endif

  log_debug("user out trade no '%s' done!\n",tno);

  ret = 0;

__done:

  if (!out_conn->ssl || out_conn->ssl->state==s_ok) {
    if (!alipay_tx(net,out_conn))
      close(out_conn->fd);
    else 
      log_error("send later by %d\n",out_conn->fd);
  }

  if (pay_data)
    delete_tree_map(pay_data);
  if (reason)
    drop_dbuffer(reason);

  return ret;
}

static
int do_verify_sign(pay_data_t pd, tree_map_t user_params)
{
  tree_map_t pay_params = 0 ;
  char *pubkeypath = 0, *tmp = 0;
  dbuffer_t sign_params = 0;
  dbuffer_t sign = 0, sign_dec = 0, ud = 0;
  int ret = 0, dec_len = 0;


  pay_params = pd->pay_params ;
  pubkeypath = get_tree_map_value(pay_params,PUBKEY);

  if (!pubkeypath) {
    log_error("no public key path defined\n");
    return -1;
  }

  tmp = get_tree_map_value(user_params,SIGN);
  if (unlikely(!tmp)) {
    log_error("no '%s'",SIGN);
    return -1;
  }

  // save 'sign'
  sign = alloc_default_dbuffer();
  write_dbuf_str(sign,tmp);

  // url decode the signature
  ud = alloc_default_dbuffer();
  url_decode(sign,strlen(sign),&ud);

  // base64 decode the signature
  dec_len  = strlen(ud)+2;
  sign_dec = alloc_dbuffer(dec_len);
  base64_decode((unsigned char*)ud,strlen(ud),(unsigned char*)sign_dec,&dec_len);

  // remove 'sign' and 'sign_type'
  drop_tree_map_item(user_params,SIGN,strlen(SIGN));
  drop_tree_map_item(user_params,SIGNTYPE,strlen(SIGNTYPE));
  sign_params = create_html_params(user_params);

  // url decode the plain text
  write_dbuf_str(ud,"");
  url_decode(sign_params,strlen(sign_params),&ud);

  if (rsa_public_verify(pubkeypath,ud,(unsigned char*)sign_dec,dec_len)!=1) {
    log_error("verify sign fail\n");
    ret = -1;
  }

  drop_dbuffer(sign_params);
  drop_dbuffer(sign_dec);
  drop_dbuffer(sign);
  drop_dbuffer(ud);

  return ret;
}

static
order_info_t get_order_by_otn(const char *tno, dbuffer_t *errbuf, bool *bRel)
{
  paySvr_config_t pc = get_running_configs();
  mysql_conf_t mcfg = get_mysql_configs(pc);
  rds_order_entry_t pre = &g_alipayData.m_rOrders;
  order_entry_t pe = get_order_entry();
  order_info_t po = get_order_by_outTradeNo(pe,(char*)tno);


  if (!po) {
    int ret = 0;
    // try query redis cache
    dbuffer_t orderid = alloc_default_dbuffer();
    if (get_rds_order_index(pre,mcfg->order_table,tno,&orderid) || 
        !(po = get_rds_order(pre,mcfg->order_table,orderid,false))) {
      FORMAT_ERR(errbuf,"found no order info by out_trade_no '%s'!\n",tno);
      ret = -2;
    }
    else {
      *bRel = true ;
    }

    drop_dbuffer(orderid);

    if (ret==-2)
      return NULL ;
  }

  return po ;
}

static 
int do_alipay_notify(Network_t net,connection_t pconn,tree_map_t user_params)
{
  const char *alipay_notify_res = "success";
  char *tno = get_tree_map_value(user_params,OTNO);
  //order_entry_t pe = get_order_entry();
  rds_order_entry_t pre = &g_alipayData.m_rOrders;
  paySvr_config_t pc = get_running_configs();
  mysql_conf_t mcfg = get_mysql_configs(pc);
  pay_channels_entry_t pce = get_pay_channels_entry() ;
  const char *appid = get_tree_map_value(user_params,APPID);
  const char *payChan = action__alipay_order.channel ;
  pay_data_t pd = 0 ;
  order_info_t po = 0;
  connection_t out_conn = 0;
  tree_map_t notify = 0;
  char tmp[64] = "", *ptr = 0;
  int param_type = pt_json ;
  merchant_entry_t pme = get_merchant_entry();
  merchant_info_t pm = 0;
  int ret = -1, st = 0;
  bool rel = false ;


  // FIXME: verify params 

  pd = get_paydata_by_ali_appid(pce,payChan,appid);
  if (!pd) {
    log_error("found no pay route by '%s'\n",payChan);
    return -1;
  }

  // verify signature of alipay notifications
  if (do_verify_sign(pd,user_params)) {
    return -1;
  }

  // send a feed back to ALI
  create_http_normal_res(&pconn->txb,pt_html,alipay_notify_res);

  if (!pconn->ssl || pconn->ssl->state==s_ok) {
    if (!alipay_tx(net,pconn))
      close(pconn->fd);
    else 
      log_error("send later by %d\n",pconn->fd);
  }

  if (!tno) {
    log_error("no 'out_trade_no' field found\n");
    return -1;
  }

  po = get_order_by_otn(tno,NULL,&rel);
  if (!po)
    goto __done ;

  if (!(pm=get_merchant(pme,po->mch.no))) {
    log_error("no such merchant '%s'\n",po->mch.no);
    goto __done;
  }

  // update risk control arguments
  update_paydata_rc_arguments(pd,po->amount);

  ptr = get_tree_map_value(pm->mch_info,PARAM_TYPE);
  if (ptr) {
    param_type = !strcmp(ptr,"json")?pt_json:pt_html ;
  }

  // update order status by ALI status
  ptr = get_tree_map_value(user_params,"trade_status");

  st = !strcmp(ptr,"TRADE_SUCCESS")?s_paid:s_err;

  set_order_message(po,ptr);

  // update order status
  set_order_status(po,st);

  // user notifying status
  set_order_un_status(po,s_notifying);

  // update redis
  if (save_rds_order(pre,mcfg->order_table,po)) {
    log_error("update order '%s' to redis fail\n",po->id);
  }

  /* connect merchant server(treat as 'backend')  */
  out_conn = do_out_connect(net,NULL,po->mch.notify_url,po);
  if (!out_conn) {
    log_error("connection to '%s' fail\n",po->mch.notify_url);
    goto __done;
  }

  log_debug("connecting to '%s' ok!\n",po->mch.notify_url);

  // construct the 'merchant notify'
  notify = new_tree_map();
  put_tree_map_string(notify,TNO,po->id);
  put_tree_map_string(notify,OTNO,po->mch.out_trade_no);
  put_tree_map_string(notify,STATUS,get_pay_status_str(po->status));
  snprintf(tmp,sizeof(tmp),"%s%.2f",param_type==pt_json?"$":"",po->amount);
  put_tree_map_string(notify,AMT,tmp);

  create_http_post_req(&out_conn->txb,po->mch.notify_url,pt_json,notify);

  delete_tree_map(notify);

  if (!out_conn->ssl || out_conn->ssl->state==s_ok) {
    alipay_tx(net,out_conn);
  }

  ret = 0;

__done:
  if (rel)
    release_rds_order(pre,po);

  return ret;
}

static 
int do_alipay_query(Network_t net,connection_t pconn,tree_map_t user_params)
{
  char *tno = get_tree_map_value(user_params,OTNO);
  rds_order_entry_t pre = &g_alipayData.m_rOrders;
  order_info_t po = 0;
  tree_map_t qry_res = 0;
  char tmp[64] = "", *ptr = 0;
  merchant_entry_t pme = get_merchant_entry();
  merchant_info_t pm = 0;
  int param_type = pt_json ;
  dbuffer_t *errbuf = &pconn->txb;
  int ret = -1;
  bool bRel = false ;


  // TODO: verify merchant signature!!

  ptr = get_tree_map_value(user_params,MCHID);
  if (!ptr || !(pm=get_merchant(pme,ptr))) {
    FORMAT_ERR(errbuf,"no such merchant '%s'\n",ptr);
    goto __done ;
  }

  if (!tno) {
    FORMAT_ERR(errbuf,"no 'out_trade_no' field supplied!\n");
    goto __done ;
  }

  po = get_order_by_otn(tno,errbuf,&bRel);
  if (!po) 
    goto __done ;

  ptr = get_tree_map_value(pm->mch_info,PARAM_TYPE);
  if (ptr) {
    param_type = !strcmp(ptr,"json")?pt_json:pt_html ;
  }

  qry_res = new_tree_map();
  put_tree_map_string(qry_res,TNO,po->id);
  put_tree_map_string(qry_res,OTNO,po->mch.out_trade_no);
  put_tree_map_string(qry_res,STATUS,get_pay_status_str(po->status));
  snprintf(tmp,sizeof(tmp),"%s%.02f",param_type==pt_json?"$":"",po->amount);
  put_tree_map_string(qry_res,AMT,tmp);
  snprintf(tmp,sizeof(tmp),"%s%lld",param_type==pt_json?"$":"",po->create_time);
  put_tree_map_string(qry_res,"create_time",tmp);

  create_http_normal_res2(&pconn->txb,param_type,qry_res);
  delete_tree_map(qry_res);

  ret = 0;

__done:

  if (!pconn->ssl || pconn->ssl->state==s_ok) {
    if (!alipay_tx(net,pconn))
      close(pconn->fd);
    else 
      log_error("send later by %d\n",pconn->fd);
  }

  if (bRel)
    release_rds_order(pre,po);

  return ret;
}


static int dynamic_cfg_updater(void *pnet, void *ptos)
{
  dbuffer_t *pmsg = &g_alipayData.du.push_msg ;


  while (!myredis_get_push_msg(&g_alipayData.du.m_rds,pmsg) && 
         dbuffer_data_size(*pmsg)>0L) {
    log_debug("fetch mq msg: %s(%zu)\n",*pmsg,dbuffer_data_size(*pmsg));
  }

  return 0;
}


static
struct simple_timer_s g_cfg_updater = 
{
  .desc = "dynamic config updater",

  .cb = dynamic_cfg_updater,

  .timeouts = /*60*/1,
} ;

static 
int init_dynamic_cfg_updater(const char *host, int port, const char *name)
{
  extern void add_external_timer(simple_timer_t t);
  int ret = myredis_init(&g_alipayData.du.m_rds,host,port,(char*)name);


  if (ret)
    return -1;

  g_alipayData.du.push_msg = alloc_default_dbuffer();

  add_external_timer(&g_cfg_updater);

  log_info("dynamic config updater init Ok!\n");

  return 0;
}

static
void destroy_dynamic_cfg_updater()
{
  myredis_release(&g_alipayData.du.m_rds);

  drop_dbuffer(g_alipayData.du.push_msg);
}

static
struct http_action_s action__alipay_order = 
{
  .key = "alipay/pay",
  .channel = "alipay",
  .action  = "pay",
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
struct http_action_s action__alipay_query = 
{
  .key = "alipay/query",
  .channel = "alipay",
  .action  = "query",
  .cb      = do_alipay_query,
} ;


static
int alipay_init(Network_t net)
{
  http_action_entry_t pe = get_http_action_entry();
  struct myredis_config_s rconf ;
  paySvr_config_t pc = get_running_configs();
  

  if (!get_myredis_configs(pc,&rconf)) {

    // init current module's redis connection
    int ret = myredis_init(&g_alipayData.m_rds, rconf.host,
                           rconf.port, rconf.cfg_cache);
    log_info("connect to redis %s:%d ... %s\n",
             rconf.host, rconf.port, ret?"fail!":"ok!");

    // redis order cache
    init_rds_order_entry(&g_alipayData.m_rOrders,g_alipayData.m_rds.ctx,rconf.order_cache);

    // auto id
    aid_init(&g_alipayData.aid,"alp",g_alipayData.m_rds.ctx);

    // for dynamic configs
    if (!ret) 
      init_dynamic_cfg_updater(rconf.host,rconf.port,rconf.cfg_cache);
  }

  add_http_action(pe,&action__alipay_order);
  add_http_action(pe,&action__alipay_notify);
  add_http_action(pe,&action__alipay_query);

  // TODO: create the 'push messages rx' thread here, 
  //  use to update configs dynamically

  log_info("init done!");

  return 0;
}

static
void alipay_release()
{
  release_all_rds_orders(&g_alipayData.m_rOrders);

  myredis_release(&g_alipayData.m_rds);

  destroy_dynamic_cfg_updater();
}

