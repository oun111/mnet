
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
#include "md.h"

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
#define UPT          "update_table"


struct alipay_data_s {

  struct auto_id_s aid ;

  struct rds_order_entry_s m_rOrders ;

  struct myredis_s m_rds ; // fetch order data

  struct myredis_s m_cfg_rds ; // fetch config data

  // used by dynamic updater
  struct dynamic_updater_s {
    dbuffer_t push_msg ;

    /* 
     * bit0: update pay channels/risk control
     * bit1: update merchant configs
     */
    int flags ; 

    struct myredis_s rds ; // fetch subscribe/publish notice
  } du ;

} g_alipayData  ;


static int alipay_init(Network_t net);

static void alipay_release();

static struct http_action_s action__alipay_order;

static order_info_t get_order_by_otn(const char *tno, dbuffer_t *errbuf, bool *bRel);


static
int alipay_tx(Network_t net, connection_t pconn)
{
  return pconn->l4opt.tx(net,pconn);
}

static
int process_transfund2user_resp(Network_t net, connection_t pconn, backend_t be, 
                                dbuffer_t resp, size_t sz)
{
  char *body = get_http_body_ptr(resp,sz);
  rds_order_entry_t pre = &g_alipayData.m_rOrders;
  paySvr_config_t pc = get_running_configs();
  mysql_conf_t mcfg = get_mysql_configs(pc);
  order_info_t po = 0;
  char *tno = be->data, *pmsg = "success" ;
  connection_t peer = be->peer ;
  int ret = -1, st = s_err;
  bool rel = false;
  tree_map_t res_map = 0;


  if (!body) {
    log_error("found no body!\n");
    goto __done;
  }

  // error page, just go back
  if (strstr(body,"<!DOCTYPE html")) {
    goto __done;
  }

  po = get_order_by_otn(tno,&peer->txb,&rel);
  if (unlikely(!po)) {
    goto __done ;
  }

  jsonKV_t *pr = jsons_parse(body);
  tree_map_t map = jsons_to_treemap(pr);

  if (!map) {
    log_debug("parse response map fail!\n");
    goto __done;
  }

  tree_map_t submap = get_tree_map_nest(map,"root");
  submap = get_tree_map_nest(submap,"alipay_fund_trans_toaccount_transfer_response");

  if (submap && get_tree_map_value(submap,"pay_date")) {
    log_debug("trans fund out biz no '%s' success!\n",tno);
    st = s_paid;
  }
  else {
    pmsg = get_tree_map_value(submap,"sub_msg");
    if (!pmsg)
      pmsg = get_tree_map_value(submap,"msg");
  }

  // update trans fund message
  set_order_message(po,pmsg);

  // update status
  set_order_status(po,st);

  // update redis
  if (save_rds_order(pre,mcfg->order_table,po)) {
    log_error("update order '%s' to redis fail\n",po->id);
  }

  // send a feed back
  res_map = new_tree_map();
  put_tree_map_string(res_map,STATUS,get_pay_status_str(po->status));
  put_tree_map_string(res_map,OTNO,po->mch.out_trade_no);
  put_tree_map_string(res_map,TNO,po->id);
  put_tree_map_string(res_map,"message",pmsg);

  create_http_normal_res2(&peer->txb,pt_json,res_map);

  if (!peer->ssl || peer->ssl->state==s_ok) {
    if (!alipay_tx(net,peer))
      close(peer->fd);
    else 
      log_error("send later by %d\n",peer->fd);
  }

  ret = 0;

__done:

  if (tno)
    free(tno);

  if (pr)
    jsons_release(pr);

  if (map)
    delete_tree_map(map);

  if (rel)
    release_rds_order(pre,po);

  if (res_map)
    delete_tree_map(res_map);

  return ret;
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

    // pay notify 
    if (be->type==bt_notify2user) {
      process_notify2user_resp(pconn,be->data,b,sz_in);
    }
    // trans fund response
    else if (be->type==bt_transfund2user) {
      process_transfund2user_resp(net,pconn,be,b,sz_in);
    }

    dbuffer_lseek(pconn->rxb,sz_in,SEEK_CUR,0);

    close(pconn->fd);
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
int do_signature(dbuffer_t *errbuf,pay_data_t pd,tree_map_t pay_data)
{
  //char *privkeypath = 0;
  dbuffer_t sign_params = 0; 
  unsigned char *sign = 0;
  //unsigned int sz_out = 0;
  size_t sz_out = 0;
  unsigned char *final_res = 0;
  int sz_res = 0;
  int ret = -1;


  // reset 'sign' field
  put_tree_map_string(pay_data,SIGN,(char*)"");

  sign_params = create_html_params(pay_data);
  //log_debug("sign string: %s, size: %zu\n",sign_params,strlen(sign_params));

#if 0
  privkeypath = get_tree_map_value(pay_params,PRIVKEY);
  if (rsa_private_sign(privkeypath,sign_params,&sign,&sz_out)!=1) {
#else
  if (rsa_private_sign1(&pd->rsa_cache,sign_params,&sign,&sz_out)!=1) {
#endif
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

  // no credit cards
  /*put_tree_map_string(pay_biz,"disable_pay_channels",
      "creditCard,credit_group");*/

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
               const char *url, void *data, int bt)
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
  create_backend(get_backend_entry(),fd,peer_conn,bt,data);

  return out_conn;
}

static 
int add_merchant_sign(merchant_info_t pm, tree_map_t params)
{
  dbuffer_t signStr = 0, outb = 0;
  int ret = -1;


  // no need to sign
  if (pm->verify_sign==false) {
    return 0;
  }

  // create sign string excluding 'sign' value
  drop_tree_map_item(params,SIGN,strlen(SIGN));
  signStr = create_html_params(params);

  // append 'key'
  append_dbuf_str(signStr,pm->pubkey);
  log_debug("sign str: %s\n",signStr);

  outb = alloc_default_dbuffer();

  // create sign
  if (do_md_sign(signStr,strlen(signStr),pm->sign_type,&outb)) {
    goto __done;
  }

  put_tree_map_string(params,SIGN,outb);

  ret = 0;

__done:
  drop_dbuffer(signStr);
  drop_dbuffer(outb);

  return ret;
}

static
int create_alipay_link(dbuffer_t *errbuf,connection_t out_conn, const char *url, 
                       merchant_info_t pm, tree_map_t pay_data)
{
  dbuffer_t wholeUrl = 0;
  dbuffer_t strParams = 0;
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

  wholeUrl = alloc_default_dbuffer();
  strParams= create_html_params(pay_data);
  
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

  // the sign 
  add_merchant_sign(pm,res_map);

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
#if 0
    FORMAT_ERR(errbuf,"no notify url supplied!\n");
    return -1;
#else
    log_error("no notify url supplied!\n");
#endif
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

static int do_dynamic_update()
{
  dbuffer_t buff = 0;
  myredis_t prds = &g_alipayData.m_cfg_rds ;
  paySvr_config_t pc = get_running_configs();
  mysql_conf_t mscfg = get_mysql_configs(pc);

  if (g_alipayData.du.flags & 0x1) {
    pay_channels_entry_t pce = 0;

    // the risk control configs
    get_remote_configs(prds,mscfg->rc_conf_table,"",&buff);
    process_rc_configs(pc,buff);
    drop_dbuffer(buff);

    // the channel alipay configs
    get_remote_configs(prds,mscfg->alipay_conf_table,"",&buff);
    process_channel_configs(pc,buff);
    drop_dbuffer(buff);

    init_pay_data(&pce);
    reset_pay_channels_entry(pce);

    g_alipayData.du.flags &= ~(0x1);
  }

  if (g_alipayData.du.flags & 0x2) {
    merchant_entry_t pme = get_merchant_entry();

    // the merchant configs
    get_remote_configs(prds,mscfg->mch_conf_table,"",&buff);
    process_merchant_configs(pc,buff);
    drop_dbuffer(buff);

    release_all_merchants(pme);
    init_merchant_data(pme);

    g_alipayData.du.flags &= ~(0x2);
  }

  return 0;
}

static
bool verify_merchant_sign(merchant_info_t pm, tree_map_t user_params, 
                          dbuffer_t *errbuf)
{
  char *signstr = 0, *sign = 0, *chkstr = 0;
  int ret = false ;


  if (!pm->verify_sign) {
    log_debug("no need verify sign\n");
    return true ;
  }

  signstr = get_tree_map_value(user_params,SIGN);
  if (!signstr) {
    FORMAT_ERR(errbuf,"no '%s' given\n",SIGN);
    return false ;
  }

  if (!pm->pubkey || strlen(pm->pubkey)==0L) {
    FORMAT_ERR(errbuf,"no key config\n");
    return false ;
  }

  sign = alloc_default_dbuffer();
  write_dbuf_str(sign,signstr);

  // create sign string excluding 'sign' value
  drop_tree_map_item(user_params,SIGN,strlen(SIGN));
  chkstr = create_html_params(user_params);

  // append 'key'
  append_dbuf_str(chkstr,pm->pubkey);
  
  if (!do_md_verify(chkstr,strlen(chkstr),sign,strlen(sign),pm->sign_type)) {
    FORMAT_ERR(errbuf,"bad sign\n");
    goto __done ;
  }

  ret = true ;

__done:
  drop_dbuffer(sign);
  drop_dbuffer(chkstr);

  return ret;
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

  // if there're new configs, do updates
  do_dynamic_update();

  mch_id = get_tree_map_value(user_params,MCHID);
  if (!mch_id || !(pm=get_merchant(pme,mch_id))) {
    FORMAT_ERR(errbuf,"no such merchant '%s'\n",mch_id);
    goto __done ;
  }

  // verify merchant signature
  if (!verify_merchant_sign(pm,user_params,errbuf)) {
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
  if (do_signature(&pconn->txb,pd,pay_data)<0) {
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
  //tree_map_t pay_params = 0 ;
  char /**pubkeypath = 0,*/ *tmp = 0;
  dbuffer_t sign_params = 0;
  dbuffer_t sign = 0, sign_dec = 0, ud = 0;
  int ret = 0, dec_len = 0;


#if 0
  pay_params = pd->pay_params ;
  pubkeypath = get_tree_map_value(pay_params,PUBKEY);

  if (!pubkeypath) {
    log_error("no public key path defined\n");
    return -1;
  }
#endif

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

  //if (rsa_public_verify(pubkeypath,ud,(unsigned char*)sign_dec,dec_len)!=1) {
  if (rsa_public_verify1(&pd->rsa_cache,ud,(unsigned char*)sign_dec,dec_len)!=1) {
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
int send_merchant_notify(Network_t net, order_info_t po)
{
  char tmp[128]  = "";
  int param_type = pt_json;
  merchant_entry_t pme = get_merchant_entry();


  // user notifying status
  set_order_un_status(po,s_notifying);

  merchant_info_t pm  = get_merchant(pme,po->mch.no);
  if (unlikely(!pm)) {
    log_error("no such merchant '%s'\n",pm->id);
    return -1;
  }

  /* connect merchant server(treat as 'backend')  */
  connection_t out_conn = do_out_connect(net,NULL,po->mch.notify_url,
                                         po,bt_notify2user);
  if (!out_conn) {
    log_error("connection to '%s' fail\n",po->mch.notify_url);
    set_order_un_status(po,s_notify_fail);
    return -1;
  }

  char *ptr = get_tree_map_value(pm->mch_info,PARAM_TYPE);
  if (ptr) {
    param_type = !strcmp(ptr,"json")?pt_json:pt_html ;
  }

  // construct the 'merchant notify'
  tree_map_t notify = new_tree_map();
  put_tree_map_string(notify,TNO,po->id);
  put_tree_map_string(notify,OTNO,po->mch.out_trade_no);
  put_tree_map_string(notify,STATUS,get_pay_status_str(po->status));
  snprintf(tmp,sizeof(tmp),"%s%.2f",param_type==pt_json?"$":"",po->amount);
  put_tree_map_string(notify,AMT,tmp);

  // the sign 
  add_merchant_sign(pm,notify);

  create_http_post_req(&out_conn->txb,po->mch.notify_url,pt_json,notify);

  if (!out_conn->ssl || out_conn->ssl->state==s_ok) {
    alipay_tx(net,out_conn);
  }

  delete_tree_map(notify);

  return 0;
}

static 
int do_alipay_notify(Network_t net,connection_t pconn,tree_map_t user_params)
{
  const char *alipay_notify_res = "success";
  rds_order_entry_t pre = &g_alipayData.m_rOrders;
  pay_channels_entry_t pce = get_pay_channels_entry() ;
  paySvr_config_t pc = get_running_configs();
  mysql_conf_t mcfg = get_mysql_configs(pc);
  const char *appid = get_tree_map_value(user_params,APPID);
  const char *payChan = action__alipay_order.channel ;
  char *tno = 0;
  pay_data_t pd = 0 ;
  order_info_t po = 0;
  char *ptr = 0;
  merchant_entry_t pme = get_merchant_entry();
  merchant_info_t pm = 0;
  int ret = -1, st = s_err;
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

  tno = get_tree_map_value(user_params,OTNO);
  if (!tno) {
    log_error("no 'out_trade_no' field found\n");
    return -1;
  }

  po = get_order_by_otn(tno,NULL,&rel);
  if (!unlikely(po))
    goto __done ;

  pm = get_merchant(pme,po->mch.no);
  if (unlikely(!pm)) {
    log_error("no such merchant '%s'\n",po->mch.no);
    goto __done;
  }

  // update risk control arguments
  update_paydata_rc_arguments(pd,po->amount);

  // update order status by ALI status
  ptr = get_tree_map_value(user_params,"trade_status");

  if (likely(ptr)) 
    st = !strcmp(ptr,"TRADE_SUCCESS")?s_paid:s_err;

  set_order_message(po,ptr);

  // update order status
  set_order_status(po,st);

  ret = 0;

  if (send_merchant_notify(net,po)) {
    ret = -1;
  }

__done:
  // update redis
  save_rds_order(pre,mcfg->order_table,po);

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


  // if there're new configs, do updates
  do_dynamic_update();

  ptr = get_tree_map_value(user_params,MCHID);
  if (!ptr || !(pm=get_merchant(pme,ptr))) {
    FORMAT_ERR(errbuf,"no such merchant '%s'\n",ptr);
    goto __done ;
  }

  // verify merchant signature
  if (!verify_merchant_sign(pm,user_params,errbuf)) {
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

  // the sign 
  add_merchant_sign(pm,qry_res);

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

static 
int do_alipay_manual_notify(Network_t net,connection_t pconn,tree_map_t user_params)
{
  const char *mnote_res = "success";
  char *tno = 0, *ptr = 0;
  rds_order_entry_t pre = &g_alipayData.m_rOrders;
  paySvr_config_t pc = get_running_configs();
  mysql_conf_t mcfg = get_mysql_configs(pc);
  dbuffer_t *errbuf = &pconn->txb;
  merchant_entry_t pme = get_merchant_entry();
  merchant_info_t pm = 0;
  order_info_t po = 0;
  int ret = 0;
  bool rel = false ;


  // if there're new configs, do updates
  do_dynamic_update();

  ptr = get_tree_map_value(user_params,MCHID);
  if (!ptr || !(pm=get_merchant(pme,ptr))) {
    FORMAT_ERR(errbuf,"no such merchant '%s'\n",ptr);
    return -1;
  }

  // verify merchant signature
  if (!verify_merchant_sign(pm,user_params,errbuf)) {
    return -1;
  }

  // send a feed back
  create_http_normal_res(&pconn->txb,pt_html,mnote_res);

  if (!pconn->ssl || pconn->ssl->state==s_ok) {
    if (!alipay_tx(net,pconn))
      close(pconn->fd);
    else 
      log_error("send later by %d\n",pconn->fd);
  }

  tno = get_tree_map_value(user_params,OTNO);
  if (!tno) {
    log_error("no 'out_trade_no' field found\n");
    return -1;
  }

  log_debug("try to send manual notice by order '%s'\n",tno);

  po = get_order_by_otn(tno,NULL,&rel);
  if (!po)
    return -1 ;

  if (send_merchant_notify(net,po)) {
    ret = -1 ;
  }

  // update redis
  save_rds_order(pre,mcfg->order_table,po);

  if (rel)
    release_rds_order(pre,po);

  return ret;
}

static 
int update_alipay_transfund_biz(dbuffer_t *errbuf, tree_map_t user_params, 
                                tree_map_t pay_params, tree_map_t pay_data)
{
  tree_map_t pay_biz = NULL;
  time_t curr = time(NULL);
  struct tm *tm = localtime(&curr);
  char tmp[96] = "";
  char *payee_account = 0, /**payer_name = 0,*/ *out_biz_no = 0, 
       *amount=0, *orderid = 0, *payee_name = 0, *remark = "remark";


  payee_account = get_tree_map_value(user_params,"payee_account");
  if (!payee_account) {
    FORMAT_ERR(errbuf,"no 'payee_account' found\n");
    return -1;
  }

#if 0
  payer_name = get_tree_map_value(user_params,"payer_show_name");
  if (!payer_name) {
    FORMAT_ERR(errbuf,"no 'payer_show_name' found\n");
    return -1;
  }
#endif

  payee_name = get_tree_map_value(user_params,"payee_real_name");
  if (!payee_name) {
    FORMAT_ERR(errbuf,"no 'payee_real_name' found\n");
    return -1;
  }

  amount = get_tree_map_value(user_params,TAMT);
  if (!amount) {
    FORMAT_ERR(errbuf,"no '%s' found\n",TAMT);
    return -1;
  }

  // user out_biz_no to be alipay request order id
  out_biz_no = get_tree_map_value(user_params,OTNO);
  if (!out_biz_no) {
    FORMAT_ERR(errbuf,"no 'out_biz_no' found\n");
    return -1;
  }

  remark = get_tree_map_value(user_params,"remark");
  if (!remark) {
    log_error("no 'remark' specified\n");
    remark = "remark";
  }

  orderid = aid_add_and_fetch(&g_alipayData.aid,1L);
  log_debug("create new trans-fund order id: %s\n",orderid);


  pay_biz = new_tree_map();

  snprintf(tmp,sizeof(tmp),"%04d-%02d-%02d %02d:%02d:%02d",tm->tm_year+1900,
           tm->tm_mon+1,tm->tm_mday,tm->tm_hour,tm->tm_min,
           tm->tm_sec);
  put_tree_map_string(pay_data,"timestamp",tmp);

  put_tree_map_string(pay_data,APPID,
      get_tree_map_value(pay_params,APPID));

  put_tree_map_string(pay_data,"method",
      "alipay.fund.trans.toaccount.transfer");

  put_tree_map_string(pay_data,"format",
      get_tree_map_value(pay_params,"format"));

  put_tree_map_string(pay_data,"charset",
      get_tree_map_value(pay_params,"charset"));

  put_tree_map_string(pay_data,SIGNTYPE,
      get_tree_map_value(pay_params,SIGNTYPE));

  put_tree_map_string(pay_data,"version",
      get_tree_map_value(pay_params,"version"));

  put_tree_map_string(pay_biz,"out_biz_no",out_biz_no);
  put_tree_map_string(pay_biz,"payee_type","ALIPAY_LOGONID");
  put_tree_map_string(pay_biz,"payee_account",payee_account);

  // $ express an integer
  snprintf(tmp,sizeof(tmp),"$%s",amount);
  put_tree_map_string(pay_biz,"amount",tmp);

  //put_tree_map_string(pay_biz,"payer_show_name","");
  put_tree_map_string(pay_biz,"payee_real_name",payee_name);
  put_tree_map_string(pay_biz,"remark",remark);


  dbuffer_t strBiz = create_json_params(pay_biz);
  put_tree_map_string(pay_data,"biz_content",strBiz);


  drop_dbuffer(strBiz);
  delete_tree_map(pay_biz);

  return 0;
}

static 
int do_alipay_trans_fund(Network_t net,connection_t pconn,tree_map_t user_params)
{
  char *url = 0;
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
  tree_map_t tf_data = 0;


  log_debug("user out biz no: '%s'\n",tno);

  // if there're new configs, do updates
  do_dynamic_update();

  mch_id = get_tree_map_value(user_params,MCHID);
  if (!mch_id || !(pm=get_merchant(pme,mch_id))) {
    FORMAT_ERR(errbuf,"no such merchant '%s'\n",mch_id);
    return -1 ;
  }

  // verify merchant signature
  if (!verify_merchant_sign(pm,user_params,errbuf)) {
    return -1 ;
  }

  reason = alloc_default_dbuffer();

  // get suitable trans-fund route
  pd = get_trans_fund_route(get_pay_channels_entry(),payChan,&reason);
  if (!pd) {
    FORMAT_ERR(errbuf,"no trans fund route for channel '%s', reason: %s\n",
               payChan,reason);
    goto __done ;
  }

  log_debug("with app_id: %s\n",get_tree_map_value(pd->pay_params,APPID));

  pay_params = pd->pay_params ;
  url = get_tree_map_value(pay_params,REQ_URL);

  if (unlikely(!url)) {
    FORMAT_ERR(errbuf,"no 'url' configs\n");
    goto __done ;
  }

  tf_data = new_tree_map();

  if (update_alipay_transfund_biz(&pconn->txb,user_params,pay_params,tf_data)) {
    goto __done ;
  }

  if (create_order(&pconn->txb,pay_params,user_params)) {
    goto __done ;
  }

  // deals with cryptographic
  if (do_signature(&pconn->txb,pd,tf_data)<0) {
    goto __done ;
  }

  /* connect alipay server(treat as 'backend')  */
  connection_t out_conn = do_out_connect(net,pconn,url,strdup(tno),bt_transfund2user);
  if (unlikely(!out_conn)) {
    log_error("connection to '%s' fail\n",url);
    goto __done;
  }

  // send request
  create_http_get_req(&out_conn->txb,url,pt_html,tf_data);

  if (!out_conn->ssl || out_conn->ssl->state==s_ok) {
    if (!alipay_tx(net,out_conn))
      close(out_conn->fd);
    else 
      log_error("send later by %d\n",out_conn->fd);
  }

  log_debug("user out biz no '%s' done!\n",tno);

  ret = 0;

__done:

  if (tf_data)
    delete_tree_map(tf_data);
  if (reason)
    drop_dbuffer(reason);

  return ret;
}

static 
int do_alipay_cfg_update(Network_t net,connection_t pconn,tree_map_t user_params)
{
  myredis_t prds = &g_alipayData.m_cfg_rds ;
  char *t = get_tree_map_value(user_params,UPT);
  dbuffer_t *errbuf = &pconn->txb;
  const char *du_res = "success";
  int ret = -1;


  if (!t) {
    FORMAT_ERR(errbuf,"no '%s' field supplied!\n",UPT);
    goto __done ;
  }

  // remove old configs 
  if (myredis_hdel_cache(prds,t,NULL)) {
    FORMAT_ERR(errbuf,"hdel on redis fail!\n");
    goto __done ;
  }

  // notify all worker processes
  if (myredis_publish_msg(prds,t)) {
    FORMAT_ERR(errbuf,"publish to redis fail!\n");
    goto __done ;
  }

  // send a feed back
  create_http_normal_res(&pconn->txb,pt_html,du_res);

  ret = 0;

__done:

  if (!pconn->ssl || pconn->ssl->state==s_ok) {
    if (!alipay_tx(net,pconn))
      close(pconn->fd);
    else 
      log_error("send later by %d\n",pconn->fd);
  }

  return ret;
}

static int fetch_du_notice(void *pnet, void *ptos)
{
  myredis_t prds = &g_alipayData.du.rds ;
  paySvr_config_t pc = get_running_configs();
  mysql_conf_t mscfg = get_mysql_configs(pc);
  myredis_conf_t rconf = get_myredis_configs(pc);
  dbuffer_t *pmsg = &g_alipayData.du.push_msg ;
  int ret = 0;
  

  // a single redis connection for subscribe/publish
  if (!is_myredis_ok(prds)) {
    ret = myredis_init(prds,rconf->host,rconf->port,
                       rconf->cfg_cache);

    if (ret) {
      log_error("connect to redis %s:%d fail!\n",
                rconf->host, rconf->port);
      return -1;
    }
  }

  while (!myredis_subscribe_msg(prds,pmsg) && 
         dbuffer_data_size(*pmsg)>0L) {

    if (!strcasecmp(*pmsg,mscfg->rc_conf_table) || 
        !strcasecmp(*pmsg,mscfg->alipay_conf_table)) {
      g_alipayData.du.flags |= 0x1;
      log_debug("updating '%s'...\n",*pmsg);
    }

    if (!strcasecmp(*pmsg,mscfg->mch_conf_table)) {
      g_alipayData.du.flags |= 0x2;
      log_debug("updating '%s'...\n",*pmsg);
    }
  }

  //myredis_release(&rds);

  return 0;
}

static
struct simple_timer_s g_cfg_updater = 
{
  .desc = "dynamic config updater",

  .cb = fetch_du_notice,

  .timeouts = 60,
} ;

static int init_dynamic_cfg_updater()
{
  extern void add_external_timer(simple_timer_t t);
  paySvr_config_t pc = get_running_configs();
  myredis_conf_t rconf = get_myredis_configs(pc);


  g_alipayData.du.push_msg = alloc_default_dbuffer();

  g_alipayData.du.flags = 0;

  g_cfg_updater.timeouts = rconf->cfg_scan_interval ;
  log_info("redis config scan timeout: %ds\n",g_cfg_updater.timeouts);

  add_external_timer(&g_cfg_updater);

  log_info("dynamic config updater init Ok!\n");

  return 0;
}

static
void destroy_dynamic_cfg_updater()
{
  drop_dbuffer(g_alipayData.du.push_msg);

  myredis_release(&g_alipayData.du.rds);
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
struct http_action_s action__alipay_manualNotify = 
{
  .key = "alipay/mnote2",
  .channel = "alipay",
  .action  = "mnote2",
  .cb      = do_alipay_manual_notify,
} ;

static
struct http_action_s action__alipay_transFund = 
{
  .key = "alipay/transfund",
  .channel = "alipay",
  .action  = "transfund",
  .cb      = do_alipay_trans_fund,
} ;

static
struct http_action_s action__alipay_cfgUpdate = 
{
  .key = "alipay/cfgUpdate",
  .channel = "alipay",
  .action  = "cfgUpdate",
  .cb      = do_alipay_cfg_update,
} ;


static
int alipay_init(Network_t net)
{
  http_action_entry_t pe = get_http_action_entry();
  paySvr_config_t pc = get_running_configs();
  myredis_conf_t rconf = get_myredis_configs(pc) ;
  int ret = 0;
  
  // init current module's redis connection
  ret = myredis_init(&g_alipayData.m_rds, rconf->host,
                     rconf->port, rconf->order_cache);
  log_info("connect to redis %s:%d ... %s\n",
           rconf->host, rconf->port, ret?"fail!":"ok!");

  // share same redis connection with 'g_alipayData.m_rds', 
  //  but access 'cfg cache' instead of 'order cache'
  myredis_dup(&g_alipayData.m_rds,&g_alipayData.m_cfg_rds,
              rconf->cfg_cache);

  // redis order cache
  init_rds_order_entry(&g_alipayData.m_rOrders,&g_alipayData.m_rds,
                       rconf->order_cache);

  // auto id
  aid_init(&g_alipayData.aid,"alp",&g_alipayData.m_rds);

  // 
  // for dynamic configs: we have 2 choice of dynamically updating
  //  configs now, the 'redis subscribe' and 'http action', BUT
  //  the 'http' way dose NOT guarantee notifying all paysvr instances
  //  under multi-processes conditions
  //
  init_dynamic_cfg_updater();

  add_http_action(pe,&action__alipay_order);
  add_http_action(pe,&action__alipay_notify);
  add_http_action(pe,&action__alipay_query);
  add_http_action(pe,&action__alipay_manualNotify);
  add_http_action(pe,&action__alipay_transFund);
  add_http_action(pe,&action__alipay_cfgUpdate);

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

