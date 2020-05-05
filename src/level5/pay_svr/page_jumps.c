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
//#include "auto_id.h"
//#include "L4.h"
#include "url_coder.h"
//#include "timer.h"
//#include "md.h"
//#include "errdef.h"
#include "qrcode_cache.h"


const char pay_page[] = 
"<html>"
"<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\" />"
"<meta http-equiv=\"X-UA-Compatible\" content=\"IE=edge,chrome=1\">"
"<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, minimum-scale=1.0, maximum-scale=1.0,user-scalable=no\" />"
"<meta http-equiv=\"Cache-Control\" content=\"no-cache\" />"
"<meta http-equiv=\"Pragma\" content=\"no-cache\" />"
"<meta http-equiv=\"Expires\" content=\"0\" />"
"<meta name=\"format-detection\" content=\"telephone=no\"/>"
"<meta name=\"apple-mobile-web-app-capable\" content=\"yes\" />"
"<meta name=\"apple-mobile-web-app-status-bar-style\" content=\"black\">"

"<style type=\"text/css\"> .div1{overflow: auto; width: 100%;height: 100%;margin:20px auto;text-align:center;vertical-align: middle;  }  </style>"

"<body style=\"text-align:center; \">"
"<div class=\"div1\">"
"<img src=\"data:image/jpeg;base64,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\">"

"<br><br>"
"<span style=\"color:#000000; font-size:20px; \"> 请支付 </span>"
"<span style=\"color:#EA0000; font-size:35px; \"> %.02f </span>"
"<span style=\"color:#000000; font-size:20px; \"> 元 </span>"
"<br><br>"
"<input style=\"height:70px;width:100%;font-size:30px; background-color:#0080FF; color:#FFFFFF;\" type=\"button\" value=\"支付宝支付\" name=\"test\" onclick=\"location.href='%s'\"/>"
"<br><br>"
"<br><br>"
"<a style=\"color:#0000C6; font-size:20px; width:100%;\" class=\"showContent\" id=\"showContent\" >若无法支付，请点击扫码</a>"
"<br><br>"

"<div id=\"code\" class=\"code\" style=\"display:none; width:100%;\"></div>"
"<script type=\"text/javascript\" src=\"https://apps.bdimg.com/libs/jquery/2.1.4/jquery.min.js\"></script>"
"<script type=\"text/javascript\" src=\"http://cdn.staticfile.org/jquery.qrcode/1.0/jquery.qrcode.min.js\"></script>"
"<script>$(\"#code\").qrcode({text:\"%s\",width:250,height:250})</script>"
"<script type=\"text/javascript\"> $(document).ready(function(){ $(\".showContent\").click(function(){ var obj=document.getElementById(\"showContent\"); var attr= obj.getAttribute(\"on\"); if (attr==\"on\") { $(\".code\").hide(); obj.removeAttribute(\"on\"); obj.innerHTML=\"若无法支付，请点击扫码\"; } else { $(\".code\").show(); obj.setAttribute(\"on\",\"on\"); obj.innerHTML=\"请截屏存相册用支付宝扫码\"; }  }); });</script>"
"</div>"

"</body>"
"</html>";


const char multi_pay_err_page[] = 
"<html>"
"<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\" />"
"<br><br>"
"<a style=\"font-size:30px\">订单号重复。请重新下单。</a>"
"<br><br>"
"</html>";

#define ORDERID      "orderid"


static struct http_action_s action__alipay_ppay2 ;
static struct http_action_s action__alipay_ppay3 ;

extern int alipay_tx(Network_t net, connection_t pconn);
extern const char* alipay_mod_name();
extern rds_order_entry_t get_rds_order_entry();
extern qrcode_cache_t get_qrcode_cache_entry();


static inline
int ptrans2_page_return(Network_t net,connection_t pconn,const char *url,
    const char *tno,tree_map_t pay_data,int bt,merchant_info_t pm) 
{
  int ret = 0;
  dbuffer_t strParams = create_html_params(pay_data);
  dbuffer_t alipaysUrl= alloc_default_dbuffer();

  
  write_dbuf_str(alipaysUrl,"alipays://platformapi/startapp?");
  append_dbuf_str(alipaysUrl,strParams);

  create_browser_redirect_req2(&pconn->txb,alipaysUrl);

  drop_dbuffer(strParams);
  drop_dbuffer(alipaysUrl);

  return ret;
}

static inline
int ptrans3_page_return(Network_t net,connection_t pconn,const char *url,
    const double amount,tree_map_t pay_data,int bt,merchant_info_t pm) 
{
  int ret = 0;
  char *page = (char*)pay_page;
  size_t pbsz = strlen(page)+(strlen(url)<<1)+10;
  dbuffer_t pagebuf= alloc_dbuffer(pbsz);

  
  snprintf(pagebuf,pbsz,page,amount,url,url);

  create_http_normal_res(&pconn->txb,-1,pt_html,pagebuf);

  drop_dbuffer(pagebuf);

  return ret;
}

static inline
order_info_t get_order_by_id(char *odrid, bool *del)
{
  order_entry_t pe = get_order_entry();
  rds_order_entry_t pre = get_rds_order_entry();
  paySvr_config_t pc = get_running_configs();
  mysql_conf_t mcfg = get_mysql_configs(pc);
  order_info_t po = get_order(pe,odrid);


  *del = false ;

  if (!po) {
    po = get_rds_order(pre,mcfg->order_table,odrid,false);
    *del = po!=NULL ;
  }

  return po;
}

static 
int do_alipay_ppay2(Network_t net,connection_t pconn,tree_map_t user_params)
{
  char *url = 0;
  connection_t out_conn = pconn ;
  tree_map_t pay_data  = NULL ;
  tree_map_t pay_biz   = NULL ;
  //pay_data_t pd = 0;
  char *odrid = get_tree_map_value(user_params,ORDERID);
  //tree_map_t pay_params = NULL ;
  //dbuffer_t *errbuf = &pconn->txb;
  merchant_entry_t pme = get_merchant_entry();
  merchant_info_t pm = 0;
  //char *mch_id = NULL;
  //unsigned int pt = 0;
  int ret = -1;
  order_info_t po = 0;
  rds_order_entry_t pre = get_rds_order_entry();
  paySvr_config_t pc = get_running_configs();
  mysql_conf_t mcfg = get_mysql_configs(pc);
  char tmp[64]="";
  bool rel = false ;


  if (!odrid) {
    //log_error("no 'orderid' found!\n");
    sock_close(pconn->fd);
    goto __done;
  }

  log_debug("requesting trade no: '%s'\n",odrid);

  if (!(po=get_order_by_id(odrid,&rel))) {
    sock_close(pconn->fd);
    goto __done;
  }

  if (po->status==s_paid || po->status==s_paying) {
    log_error("duplicated paying order '%s' !\n",po->mch.no);
    create_http_normal_res(&pconn->txb,-1,pt_html,multi_pay_err_page);
    goto __done ;
  }

  if (unlikely(!(pm=get_merchant(pme,po->mch.no)))) {
    log_error("no such merchant '%s' !\n",po->mch.no);
    goto __done ;
  }

  pay_biz = new_tree_map();
  pay_data= new_tree_map();

  // 个人转帐固定写死，也可以09999988
  put_tree_map_string(pay_data,"appId","20000123");
  put_tree_map_string(pay_data,"actionType","scan");

  // fixed
  put_tree_map_string(pay_biz,"s","money");
  // buyer_id / pid
  put_tree_map_string(pay_biz,"u",po->chan.mch_no);
  // amount
  snprintf(tmp,sizeof(tmp),"%f",po->amount);
  put_tree_map_string(pay_biz,"a",tmp);
  // out-trade-no
  snprintf(tmp,sizeof(tmp),"备注 %s",po->mch.out_trade_no);
  put_tree_map_string(pay_biz,"m",tmp);

  dbuffer_t strBiz = create_json_params(pay_biz);

  put_tree_map_string(pay_data,"biz_data",strBiz);

  drop_dbuffer(strBiz);
  delete_tree_map(pay_biz);

  // update status
  set_order_status(po,s_paying);
  if (save_rds_order(pre,mcfg->order_table,po)) {
    log_error("update order '%s' to redis fail\n",po->id);
    goto __done ;
  }

  if (ptrans2_page_return(net,out_conn,url,NULL,pay_data,0,pm)) {
    goto __done ;
  }

  log_debug("order id '%s' done!\n",odrid);

  ret = 0;

__done:

  if (!out_conn->ssl || out_conn->ssl->state==s_ok) {
    if (!alipay_tx(net,out_conn))
      sock_close(out_conn->fd);
    else 
      log_debug("send later by %d\n",out_conn->fd);
  }

  if (pay_data)
    delete_tree_map(pay_data);
  if (rel)
    release_rds_order(pre,po);

  return ret;
}

static 
int do_alipay_ppay3(Network_t net,connection_t pconn,tree_map_t user_params)
{
  char *url = 0, *nurl = 0;
  connection_t out_conn = pconn ;
  pay_data_t pd = 0;
  char *odrid = get_tree_map_value(user_params,ORDERID);
  tree_map_t pay_params = NULL ;
  const char *payChan = alipay_mod_name() ;
  pay_channels_entry_t pce = get_pay_channels_entry() ;
  int ret = -1;
  order_info_t po = 0;
  char host[64]="";
  int port = 0;
  dbuffer_t alipaysUrl = 0;
  rds_order_entry_t pre = get_rds_order_entry();
  bool rel = false ;


  if (!odrid) {
    //log_error("no 'orderid' found!\n");
    return -1;
  }

  log_debug("requesting trade no: '%s'\n",odrid);

  if (!(po=get_order_by_id(odrid,&rel))) {
    sock_close(pconn->fd);
    goto __done;
  }

  pd = get_paydata_by_ali_appid(pce,payChan,po->chan.mch_no,
                                po->order_type);
  if (unlikely(!pd)) {
    log_error("found no pay data by '%s'\n",payChan);
    sock_close(pconn->fd);
    goto __done;
  }

  pay_params = pd->pay_params;

  url = get_tree_map_value(pay_params,"req_url");
  if (unlikely(!url)) {
    log_error("NO req url configs!!\n");
    goto __done ;
  }

  nurl = get_tree_map_value(pay_params,"notify_url");
  if (unlikely(!nurl))  {
    log_error("NO notify url configs!!\n");
    goto __done;
  }

  if (po->order_type==t_qrpay && unlikely(!po->chan.qrcode)) {
    po->chan.qrcode = alloc_default_dbuffer();
    if (qrcode_cache_fetch(get_qrcode_cache_entry(),po->id,&po->chan.qrcode)) {
      create_http_normal_res(&pconn->txb,200,pt_html,"no qrcode save!");
      goto __done;
    }
  }

  // personal transfund
  if (po->order_type==t_persTrans) {

    // parse server outer ip from notify url
    parse_http_url(nurl,host,sizeof(host),&port,NULL,0L,NULL);
    snprintf(host+strlen(host),sizeof(host)-strlen(host)+1,":%d",port);

    alipaysUrl = alloc_default_dbuffer();
    write_dbuf_str(alipaysUrl,"alipayqr://platformapi/startapp?saId=10000007&qrcode=");
    append_dbuf_str(alipaysUrl,"http://");
    append_dbuf_str(alipaysUrl,host);
    append_dbuf_str(alipaysUrl,"/");
    append_dbuf_str(alipaysUrl,alipay_mod_name());
    append_dbuf_str(alipaysUrl,"/");
    append_dbuf_str(alipaysUrl,action__alipay_ppay2.action);
    append_dbuf_str(alipaysUrl,"?");

    tree_map_t pay_data= new_tree_map();
    put_tree_map_string(pay_data,ORDERID,odrid);

    dbuffer_t strParams = create_html_params(pay_data);
    append_dbuf_str(alipaysUrl,strParams);

    delete_tree_map(pay_data);

    // url encode the 'alipays://...'
    url_encode(alipaysUrl,dbuffer_data_size(alipaysUrl),&strParams);

    write_dbuf_str(alipaysUrl,url);
    append_dbuf_str(alipaysUrl,"?scheme=");
    append_dbuf_str(alipaysUrl,strParams);
    drop_dbuffer(strParams);
    url = alipaysUrl ;
  }
  // face pay
  else {
    url = po->chan.qrcode ;
  }

  if (unlikely(!url)) {
    sock_close(pconn->fd);
    log_error("fatal: no page jump url found!");
    goto __done ;
  }

  if (ptrans3_page_return(net,out_conn,url,po->amount,NULL,0,NULL)) {
    goto __done ;
  }

  log_debug("order id '%s' done!\n",odrid);

  ret = 0;

__done:

  if (!out_conn->ssl || out_conn->ssl->state==s_ok) {
    if (!alipay_tx(net,out_conn))
      sock_close(out_conn->fd);
    else 
      log_debug("send later by %d\n",out_conn->fd);
  }

  if (alipaysUrl)
    drop_dbuffer(alipaysUrl);
  if (rel)
    release_rds_order(pre,po);

  return ret;
}

static
struct http_action_s action__alipay_ppay2 = 
{
  .action  = "ppay2",
  .cb      = do_alipay_ppay2,
} ;

static
struct http_action_s action__alipay_ppay3 = 
{
  .action  = "ppay3",
  .cb      = do_alipay_ppay3,
} ;

const char* ppay3_action_name()
{
  return action__alipay_ppay3.action;
}

int page_jump_init()
{
  http_action_entry_t pe = get_http_action_entry();

  add_http_action(pe,alipay_mod_name(),&action__alipay_ppay2);
  add_http_action(pe,alipay_mod_name(),&action__alipay_ppay3);

  return 0;
}

