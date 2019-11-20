#include <string.h>
#include "instance.h"
#include "action.h"
#include "config.h"
#include "http_svr.h"
#include "backend.h"
#include "module.h"
#include "log.h"
#include "myrbtree.h"
#include "mysqlwp.h"
#include "http_utils.h"
#include "crypto.h"
#include "base64.h"
#include "socket.h"




static
struct alipay_chk_data {

  char conf_path[PATH_MAX];

  struct alipayChk_config_s m_conf;

  struct backend_entry_s m_backends;

  struct mysql_entry_s m_mysql ;

} g_alipayChkData ;


static int do_check_alipay_chans();


alipayChk_config_t get_running_configs()
{
  return &g_alipayChkData.m_conf;
}

backend_entry_t get_backend_entry()
{
  return &g_alipayChkData.m_backends;
}

static
int alipay_chk_parse_response(char *resp, size_t sz)
{
  int ret = -1;
  char *body = get_http_body_ptr(resp,sz);
#define READ_RESP(__submap) do {\
  char *retcode = get_tree_map_value(__submap,"code"); \
  char *retmsg = get_tree_map_value(__submap,"sub_msg"); \
  log_info("command %s, message: %s\n",!strcmp(retcode,"10000")?"success":"fail", \
      retmsg); \
  ret = 0;\
} while(0)


  if (!body) {
    log_error("found no body!\n");
    goto __done;
  }

  // error page, just go back
  if (strstr(body,"<!DOCTYPE html")) {
    goto __done;
  }

  jsonKV_t *pr = jsons_parse(body);
  tree_map_t map = jsons_to_treemap(pr);

  if (!map) {
    log_debug("parse response map fail!\n");
    goto __done;
  }

  tree_map_t submap1 = get_tree_map_nest(map,"root"), submap = 0;

  submap = get_tree_map_nest(submap1,"alipay_fund_account_query_response");
  if (submap) {
    READ_RESP(submap); 
    goto __done ;
  }

  submap = get_tree_map_nest(submap1,"alipay_security_risk_customerrisk_query_response");
  if (submap) {
    READ_RESP(submap); 
    goto __done ;
  }

__done:
  if (pr)
    jsons_release(pr);

  if (map)
    delete_tree_map(map);

  return ret;
}

static
int alipay_chk_rx(Network_t net, connection_t pconn)
{
  size_t sz_in = 0L;


  if ((sz_in=http_svr_rx_raw(net,pconn))>0) {

    // collect user raw http response
    char *b = dbuffer_ptr(pconn->rxb,0);
    b[sz_in] = '\0' ;

    log_debug("rx resp: %s",b);

    dbuffer_lseek(pconn->rxb,sz_in,SEEK_CUR,0);

    alipay_chk_parse_response(b,sz_in);

    sock_close(pconn->fd);
  }

  return 0;
}

static
int alipay_chk_tx(Network_t net, connection_t pconn)
{
  return pconn->l4opt.tx(net,pconn);
}

static
int alipay_chk_init(Network_t net)
{
  // init mysql connections
  mysql_conf_t mcfg = get_mysql_configs(&g_alipayChkData.m_conf);
  mysql_entry_init(&g_alipayChkData.m_mysql,mcfg->address,mcfg->port,
                   mcfg->usr,mcfg->pwd,mcfg->db);

  init_backend_entry(&g_alipayChkData.m_backends,-1);

  // XXX: test
  do_check_alipay_chans();

  return 0;
}

static
void alipay_chk_release()
{
  mysql_entry_release(&g_alipayChkData.m_mysql);

  release_all_backends(&g_alipayChkData.m_backends);

  log_debug("done !");
}

struct module_struct_s g_alipay_chk_mod = {

  .name = "alipay_chk",

  .id = -1,

  .dyn_handle = NULL,

  .ssl = true,

  .opts[outbound_l5] = {
    .rx = alipay_chk_rx,
    .tx = alipay_chk_tx,
    .init = alipay_chk_init,
    .release = alipay_chk_release,
  },
};

static
int parse_cmd_line(int argc, char *argv[])
{
  //char *ptr = 0;

  for (int i=1; i<argc; i++) {
    if (!strcmp(argv[i],"-cc") && i+1<argc) {
      strcpy(g_alipayChkData.conf_path,argv[i+1]);
    }
    else if (!strcmp(argv[i],"-h")) {
      printf("help message\n");
      return 1;
    }
  }
  return 0;
}

static 
connection_t 
do_out_connect(Network_t net, connection_t peer_conn, 
               const char *url, void *data, int bt)
{
  bool is_ssl = true ;
  int port = 443, fd = 0 ;
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
  update_module_l4_opts(&g_alipay_chk_mod,is_ssl);

  addr = hostname_to_uladdr(host) ;

  // connect to server 
  fd = new_tcp_client(addr,port);

  if (fd<0) {
    return NULL;
  }

  out_conn = net->reg_outbound(net,fd,g_alipay_chk_mod.id);

  // save backend info
  //create_backend(get_backend_entry(),fd,peer_conn,bt,data);

  return out_conn;
}

static
int do_signature(tree_map_t pay_data, const char *privkeypath)
{
  dbuffer_t sign_params = 0; 
  unsigned char *sign = 0;
  size_t sz_out = 0;
  unsigned char *final_res = 0;
  int sz_res = 0;
  int ret = -1;


  // reset 'sign' field
  put_tree_map_string(pay_data,"sign",(char*)"");

  sign_params = create_html_params(pay_data);
  //log_debug("sign string: %s, size: %zu\n",sign_params,strlen(sign_params));

#if 1
  if (rsa_private_sign(privkeypath,sign_params,&sign,&sz_out)!=1) {
#else
  if (rsa_private_sign1(&pd->rsa_cache,sign_params,&sign,&sz_out)!=1) {
#endif
    log_error("do rsa sign fail!");
    goto __done;
  }

  sz_res = sz_out<<2 ;
  final_res = alloca(sz_res);

  if (base64_encode(sign,sz_out,final_res,&sz_res)<0) {
    log_error("do base64 encode fail!");
    goto __done;
  }

  // save signature
  put_tree_map_string(pay_data,"sign",(char*)final_res);

  urlencode_tree_map(pay_data);

  ret = 0;

__done:
  drop_dbuffer(sign_params);

  if (sign)
    free(sign);
  return ret;
}

int construct_account_query_req_params(tree_map_t pay_data, const char *app_id, 
     const char *priv_key_path)
{
  char tmp[64] = "";
  time_t curr = time(NULL);
  struct tm *tm = localtime(&curr);


  snprintf(tmp,sizeof(tmp),"%04d-%02d-%02d %02d:%02d:%02d",tm->tm_year+1900,
           tm->tm_mon+1,tm->tm_mday,tm->tm_hour,tm->tm_min,
           tm->tm_sec);
  put_tree_map_string(pay_data,"timestamp",tmp);

  put_tree_map_string(pay_data,"app_id",(char*)app_id);

  put_tree_map_string(pay_data,"method","alipay.fund.account.query");

  put_tree_map_string(pay_data,"format","JSON");

  put_tree_map_string(pay_data,"charset","utf-8");

  put_tree_map_string(pay_data,"sign_type","RSA2");

  put_tree_map_string(pay_data,"version","1.0");

  tree_map_t pay_biz = new_tree_map();
  put_tree_map_string(pay_biz,"alipay_user_id","123");
  put_tree_map_string(pay_biz,"account_type","ACCTRANS_ACCOUNT");


  dbuffer_t strBiz = create_json_params(pay_biz);
  put_tree_map_string(pay_data,"biz_content",strBiz);

  drop_dbuffer(strBiz);
  delete_tree_map(pay_biz);

  if (do_signature(pay_data,priv_key_path)) {
    return -1;
  }

  return 0;
}

int construct_risk_query_req_params(tree_map_t pay_data, const char *app_id, 
     const char *priv_key_path)
{
  char tmp[64] = "";
  time_t curr = time(NULL);
  struct tm *tm = localtime(&curr);


  snprintf(tmp,sizeof(tmp),"%04d-%02d-%02d %02d:%02d:%02d",tm->tm_year+1900,
           tm->tm_mon+1,tm->tm_mday,tm->tm_hour,tm->tm_min,
           tm->tm_sec);
  put_tree_map_string(pay_data,"timestamp",tmp);

  put_tree_map_string(pay_data,"app_id",(char*)app_id);

  put_tree_map_string(pay_data,"method","alipay.security.risk.customerrisk.query");

  put_tree_map_string(pay_data,"format","JSON");

  put_tree_map_string(pay_data,"charset","utf-8");

  put_tree_map_string(pay_data,"sign_type","RSA2");

  put_tree_map_string(pay_data,"version","1.0");

  tree_map_t pay_biz = new_tree_map();
  put_tree_map_string(pay_biz,"risk_type","riskinfo_business_license_no_isv,riskinfo_cert_no_isv");
  put_tree_map_string(pay_biz,"business_license_no","91441300MA52Y7790X");

  dbuffer_t strBiz = create_json_params(pay_biz);
  put_tree_map_string(pay_data,"biz_content",strBiz);

  drop_dbuffer(strBiz);
  delete_tree_map(pay_biz);

  if (do_signature(pay_data,priv_key_path)) {
    return -1;
  }

  return 0;
}

static int alipay_chk_biz(const char *app_id, const char *priv_key_path)
{
  Network_t net = get_current_net();
  const char *url = "https://openapi.alipay.com/gateway.do" ;
  tree_map_t pay_data = NULL;


  /* connect alipay server(treat as 'backend')  */
  connection_t out_conn = do_out_connect(net,NULL,url,NULL,-1);
  if (unlikely(!out_conn)) {
    log_error("connection to '%s' fail\n",url);
    return -1;
  }

  pay_data = new_tree_map();
  // 商户风险查询，支付宝没有开放该接口 :(
  construct_risk_query_req_params(pay_data,app_id,priv_key_path);
  // 商户可用余额查询，支付宝没有开放该接口 :(
  //construct_account_query_req_params(pay_data,app_id,priv_key_path);

  // send request
  create_http_get_req(&out_conn->txb,url,pt_html,pay_data);

  if (!out_conn->ssl || out_conn->ssl->state==s_ok) {
    if (!alipay_chk_tx(net,out_conn))
      sock_close(out_conn->fd);
    else 
      log_error("send later by %d\n",out_conn->fd);
  }

  //log_debug("user out biz no '%s' done!\n",tno);

  delete_tree_map(pay_data);

  return 0;
}

static int do_check_alipay_chans()
{
  struct llist_head res_list ;


  init_llist_head(&res_list);
  mysql_entry_get_alipay_channels(&g_alipayChkData.m_mysql,&res_list);

  mch_res_item_t pos,n;
  llist_for_each_entry(pos,res_list.first,upper) {

    //char *i = pos->app_id, *p = pos->priv_key_path;
    //log_debug("%s - %s\n",i,p);

    if (1&& strcmp(pos->app_id,"2019110568911564"))
      continue ;

    alipay_chk_biz(pos->app_id,pos->priv_key_path);
  }

  // release list items
  llist_for_each_entry_safe(pos,n,res_list.first,upper) {
    kfree(pos);
  }

  return 0;
}

void alipay_chk_module_init(int argc, char *argv[])
{
  if (parse_cmd_line(argc,argv))
    return ;

  if (init_config(&g_alipayChkData.m_conf,g_alipayChkData.conf_path))
    return ;

  register_module(&g_alipay_chk_mod);

  set_proc_name(argc,argv,"alipay_chkd");
}

void alipay_chk_module_exit()
{
  free_config(&g_alipayChkData.m_conf);
}

