
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


#define ALIPAY_DBG  0


#define FORMAT_ERR(__errbuf__,fmt,arg...) do{\
  char tmp[96]; \
  snprintf(tmp,sizeof(tmp),fmt,## arg); \
  create_http_normal_res((__errbuf__),pt_html,tmp); \
  log_error("%s",tmp);  \
} while(0)

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
int deal_crypto(tree_map_t pay_params,tree_map_t pay_data)
{
  char *privkeypath = 0;
  dbuffer_t sign_params = 0; 
  unsigned char *sign = 0;
  unsigned int sz_out = 0;
  unsigned char *final_res = 0;
  int sz_res = 0;
  int ret = 0;


  // reset 'sign' field
  put_tree_map_string(pay_data,"sign",(char*)"");

  sign_params = create_html_params(pay_data);
  //log_debug("sign string: %s, size: %zu\n",sign_params,strlen(sign_params));

  privkeypath = get_tree_map_value(pay_params,PRIVKEY);
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
int update_alipay_biz(dbuffer_t *errbuf, tree_map_t user_params, 
                      tree_map_t pay_params, tree_map_t pay_data)
{
  tree_map_t pay_biz = NULL;
  time_t curr = time(NULL);
  struct tm *tm = localtime(&curr);
  char tmp[96] = "";
  char *body = 0, *subject = 0, *out_trade_no = 0, *amount=0;


  body = get_tree_map_value(user_params,"body");
  if (!body) {
    FORMAT_ERR(errbuf,"no 'body' found\n");
    return -1;
  }

  subject = get_tree_map_value(user_params,"subject");
  if (!body) {
    FORMAT_ERR(errbuf,"no 'subject' found\n");
    return -1;
  }

  out_trade_no = aid_add_and_fetch(&g_alipayData.aid,1L);

  amount = get_tree_map_value(user_params,"total_amount");
  if (!body) {
    FORMAT_ERR(errbuf,"no 'amount' found\n");
    return -1;
  }


  pay_biz = new_tree_map();

  snprintf(tmp,sizeof(tmp),"%d-%d-%d %d:%d:%d",tm->tm_year+1900,
           tm->tm_mon+1,tm->tm_mday,tm->tm_hour,tm->tm_min,
           tm->tm_sec);
  put_tree_map_string(pay_data,"timestamp",tmp);

  put_tree_map_string(pay_data,"app_id",
      get_tree_map_value(pay_params,"app_id"));

  put_tree_map_string(pay_data,"method",
      get_tree_map_value(pay_params,"method"));

  put_tree_map_string(pay_data,"format",
      get_tree_map_value(pay_params,"format"));

  put_tree_map_string(pay_data,"charset",
      get_tree_map_value(pay_params,"charset"));

  put_tree_map_string(pay_data,"version",
      get_tree_map_value(pay_params,"version"));

  put_tree_map_string(pay_data,"notify_url",
      get_tree_map_value(pay_params,"notify_url"));

  put_tree_map_string(pay_data,"return_url",
      get_tree_map_value(pay_params,"return_url"));

  put_tree_map_string(pay_biz,"body",body);
  put_tree_map_string(pay_biz,"subject",subject);
  put_tree_map_string(pay_biz,"out_trade_no",out_trade_no);

  // $ express an integer
  snprintf(tmp,sizeof(tmp),"$%s",amount);
  put_tree_map_string(pay_biz,"total_amount",tmp);

  put_tree_map_string(pay_biz,"product_code",
      get_tree_map_value(pay_params,"product_code"));

  put_tree_map_string(pay_biz,"timeout_express",
      get_tree_map_value(pay_params,"timeout_express"));

  dbuffer_t strBiz = create_json_params(pay_biz);

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
  create_backend(get_backend_entry(),fd,peer_conn,data);

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

  ptr = get_tree_map_value(pm->mch_info,"param_type");
  if (ptr) {
    param_type = !strcmp(ptr,"json")?pt_json:pt_html ;
  }
  
  write_dbuf_str(wholeUrl,url);
  append_dbuf_str(wholeUrl,"?");
  append_dbuf_str(wholeUrl,strParams);

  set_order_status(po,s_paying);

  put_tree_map_string(res_map,"location",wholeUrl);
  put_tree_map_string(res_map,"status",get_pay_status_str(po->status));
  put_tree_map_string(res_map,"out_trade_no",po->mch.out_trade_no);
  put_tree_map_string(res_map,"trade_no",po->id);
  put_tree_map_string(res_map,"mch_id",pm->id);

  snprintf(tmp,sizeof(tmp),"%s%.2f",param_type==pt_json?"$":"",po->amount);
  put_tree_map_string(res_map,"amount",tmp);

  create_http_normal_res2(&out_conn->txb,param_type,res_map);

  drop_dbuffer(wholeUrl);
  drop_dbuffer(strParams);

  delete_tree_map(res_map);
  
  return 0;
}

static 
int create_order(dbuffer_t *errbuf,tree_map_t pay_params, tree_map_t user_params)
{
  int ret = 0;
  order_entry_t pe = get_order_entry();
  rds_order_entry_t pre = get_rds_order_entry();
  char *pOdrId = aid_fetch(&g_alipayData.aid);
  char *mch_no = get_tree_map_value(user_params,"mch_id");
  char *nurl = get_tree_map_value(user_params,"notify_url");
  char *tno = get_tree_map_value(user_params,"out_trade_no");
  char *amt = get_tree_map_value(user_params,"total_amount");
  char *chan = action__alipay_order.channel;
  char *chan_mch_no = NULL;
  paySvr_config_t pc = get_running_configs();
  mysql_conf_t mcfg = get_mysql_configs(pc);


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

  chan_mch_no = get_tree_map_value(pay_params,"app_id");

  if (get_order(pe,pOdrId)) {
    FORMAT_ERR(errbuf,"order id '%s' duplicates!\n",pOdrId);
    return -1;
  }

  // check order id from redis cache
  if (is_rds_order_exist(pre,mcfg->order_table,pOdrId)) {
    FORMAT_ERR(errbuf,"order id '%s' duplicates!!\n",pOdrId);
    return -1;
  }

  if (get_order_by_outTradeNo(pe,tno)) {
    FORMAT_ERR(errbuf,"order out trade no '%s' duplicates!\n",tno);
    return -1;
  }

  ret = save_order(pe,pOdrId,mch_no,nurl,tno,chan,chan_mch_no,atof(amt));
  if (ret) {
    FORMAT_ERR(errbuf,"save order '%s' fail!\n",pOdrId);
    return -1;
  }

  // save to redis cache
  ret = save_rds_order1(pre,mcfg->order_table,pOdrId,mch_no,nurl,tno,
                        chan,chan_mch_no,atof(amt),s_unpay);
  if (ret) {
    FORMAT_ERR(errbuf,"save redis order '%s' fail!\n",pOdrId);
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
  dbuffer_t *errbuf = &pconn->txb;
  merchant_entry_t pme = get_merchant_entry();
  merchant_info_t pm = 0;
  char *mch_id = NULL;
  int ret = 0;


  mch_id = get_tree_map_value(user_params,"mch_id");
  if (!mch_id || !(pm=get_merchant(pme,mch_id))) {
    FORMAT_ERR(errbuf,"no such merchant '%s'\n",mch_id);
    return 0;
  }

  if (!pd) {
    FORMAT_ERR(errbuf,"no pay data for channel '%s'\n",payChan);
    return -1;
  }

  pay_params = pd->pay_params ;
  url = get_tree_map_value(pay_params,REQ_URL);

  if (!url) {
    FORMAT_ERR(errbuf,"no 'url' configs\n");
    return -1;
  }

  pay_data = new_tree_map();

  if (update_alipay_biz(&pconn->txb,user_params,pay_params,pay_data)) {
    ret = -1;
    goto __done ;
  }

  if (create_order(&pconn->txb,pay_params,user_params)) {
    ret = -1;
    goto __done ;
  }

  // deals with cryptographic
  if (deal_crypto(pay_params,pay_data)<0) {
    ret = -1;
    goto __done ;
  }

#if ALIPAY_DBG==1
  int param_type = 0;
  char *str = get_tree_map_value(pay_params,PARAM_TYPE);

  if (str) {
    param_type = !strcmp(str,"html")?pt_html:pt_json;
  }

  if (create_browser_redirect_req(&out_conn->txb,url,param_type,pay_data)) {
    ret = -1;
    goto __done ;
  }
#else
  if (create_alipay_link(&pconn->txb,out_conn,url,pm,pay_data)) {
    ret = -1;
    goto __done ;
  }
#endif

  if (!out_conn->ssl || out_conn->ssl->state==s_ok) {
    alipay_tx(net,out_conn);
  }


__done:
  delete_tree_map(pay_data);

  return ret;
}


static 
int do_alipay_notify(Network_t net,connection_t pconn,tree_map_t user_params)
{
  const char *alipay_notify_res = "success";
  char *tno = get_tree_map_value(user_params,"out_trade_no");
  order_entry_t pe = get_order_entry();
  rds_order_entry_t pre = get_rds_order_entry();
  paySvr_config_t pc = get_running_configs();
  mysql_conf_t mcfg = get_mysql_configs(pc);
  order_info_t po = 0;
  connection_t out_conn = 0;
  tree_map_t notify = 0;
  char tmp[64] = "", *ptr = 0;
  int param_type = pt_json ;
  merchant_entry_t pme = get_merchant_entry();
  merchant_info_t pm = 0;
  int ret = 0;
  bool rel = false ;


  // FIXME: verify params 

  // send a feed back to ALI
  create_http_normal_res(&pconn->txb,pt_html,alipay_notify_res);

  if (!tno) {
    log_error("no 'out_trade_no' field found\n");
    return -1;
  }

  po = get_order(pe,tno);
  if (!po) {
    rel = true ;
    if (!(po=get_rds_order(pre,mcfg->order_table,tno,false))) {
      log_error("found no order info by id '%s'\n",tno);
      return -1;
    }
  }

  if (!(pm=get_merchant(pme,po->mch.no))) {
    log_error("no such merchant '%s'\n",po->mch.no);
    ret = -1;
    goto __done;
  }

  ptr = get_tree_map_value(pm->mch_info,"param_type");
  if (ptr) {
    param_type = !strcmp(ptr,"json")?pt_json:pt_html ;
  }

  /* connect merchant server(treat as 'backend')  */
  out_conn = do_out_connect(net,NULL,po->mch.notify_url,po);
  if (!out_conn) {
    log_error("connection to '%s' fail\n",po->mch.notify_url);
    ret = -1;
    goto __done;
  }

  // update order status by ALI status
  ptr = get_tree_map_value(user_params,"trade_status");
  if (!strcmp(ptr,"TRADE_SUCCESS")) {
    set_order_status(po,s_paid);
  }
  else {
    set_order_status(po,s_timeout);
  }

  // construct the 'merchant notify'
  notify = new_tree_map();
  put_tree_map_string(notify,"trade_no",po->id);
  put_tree_map_string(notify,"out_trade_no",po->mch.out_trade_no);
  put_tree_map_string(notify,"status",get_pay_status_str(po->status));
  snprintf(tmp,sizeof(tmp),"%s%.2f",param_type==pt_json?"$":"",po->amount);
  put_tree_map_string(notify,"amount",tmp);

  create_http_normal_res2(&out_conn->txb,param_type,notify);

  delete_tree_map(notify);

  if (!out_conn->ssl || out_conn->ssl->state==s_ok) {
    alipay_tx(net,out_conn);
  }

__done:
  if (rel)
    release_rds_order(pre,po);

  return ret;
}

static 
int do_alipay_query(Network_t net,connection_t pconn,tree_map_t user_params)
{
  char *tno = get_tree_map_value(user_params,"out_trade_no");
  order_entry_t pe = get_order_entry();
  rds_order_entry_t pre = get_rds_order_entry();
  paySvr_config_t pc = get_running_configs();
  mysql_conf_t mcfg = get_mysql_configs(pc);
  order_info_t po = 0;
  tree_map_t qry_res = 0;
  char tmp[64] = "", *ptr = 0;
  merchant_entry_t pme = get_merchant_entry();
  merchant_info_t pm = 0;
  int param_type = pt_json ;
  dbuffer_t *errbuf = &pconn->txb;


  ptr = get_tree_map_value(user_params,"mch_id");
  if (!ptr || !(pm=get_merchant(pme,ptr))) {
    FORMAT_ERR(errbuf,"no such merchant '%s'\n",ptr);
    return 0;
  }

  if (!tno) {
    FORMAT_ERR(errbuf,"no 'out_trade_no' field supplied!\n");
    return -1;
  }

  po = get_order_by_outTradeNo(pe,tno);
  if (!po) {
    // try query redis cache
    dbuffer_t orderid = alloc_default_dbuffer();
    if (get_rds_order_index(pre,mcfg->order_table,tno,&orderid) || 
        !(po = get_rds_order(pre,mcfg->order_table,orderid,false))) {
      FORMAT_ERR(errbuf,"found no order info by out_trade_no '%s'!\n",tno);
      drop_dbuffer(orderid);
      return -1;
    }
  }

  ptr = get_tree_map_value(pm->mch_info,"param_type");
  if (ptr) {
    param_type = !strcmp(ptr,"json")?pt_json:pt_html ;
  }

  qry_res = new_tree_map();
  put_tree_map_string(qry_res,"trade_no",po->id);
  put_tree_map_string(qry_res,"out_trade_no",po->mch.out_trade_no);
  put_tree_map_string(qry_res,"status",get_pay_status_str(po->status));
  snprintf(tmp,sizeof(tmp),"%s%.2f",param_type==pt_json?"$":"",po->amount);
  put_tree_map_string(qry_res,"amount",tmp);

  create_http_normal_res2(&pconn->txb,param_type,qry_res);

  delete_tree_map(qry_res);

  if (!pconn->ssl || pconn->ssl->state==s_ok) {
    alipay_tx(net,pconn);
  }

  return 0;
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
  

  aid_reset(&g_alipayData.aid,"alp_id");

  add_http_action(pe,&action__alipay_order);
  add_http_action(pe,&action__alipay_notify);
  add_http_action(pe,&action__alipay_query);

  return 0;
}

