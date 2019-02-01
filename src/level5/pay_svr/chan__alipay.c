
#include <time.h>
#include <string.h>
#include "pay_action.h"
#include "log.h"
#include "instance.h"
#include "module.h"
#include "socket.h"
#include "ssl.h"
#include "pay_svr.h"
#include "http_utils.h"
#include "backend.h"


static const char normalHdr[] = "HTTP/1.1 %s\r\n"
                         "Server: pay-svr/0.1\r\n"
                         //"Content-Type: text/html;charset=GBK\r\n"
                         "Content-Type: text/json;charset=GBK\r\n"
                         "Content-Length:%zu      \r\n"
                         "Date: %s\r\n\r\n";



static int alipay_ssl_outbound_init(Network_t net);


static 
int do_ok(connection_t pconn)
{
  const char *bodyPage = "{\"status\":\"success\"}\r\n" ;
  char hdr[256] = "";
  time_t t = time(NULL);
  char tb[64];

  ctime_r(&t,tb);
  snprintf(hdr,256,normalHdr,"200",strlen(bodyPage),tb);

  pconn->txb = write_dbuffer(pconn->txb,hdr,strlen(hdr));
  pconn->txb = append_dbuffer(pconn->txb,(char*)bodyPage,strlen(bodyPage));

  return 0;
}


static
int alipay_ssl_outbound_tx(Network_t net, connection_t pconn)
{
  log_debug("tx: \n");
  pconn->l4opt.tx(net,pconn);
  return 0;
}

static
int alipay_ssl_outbound_rx(Network_t net, connection_t pconn)
{
  ssize_t sz_in = 0L;
  dbuffer_t b = NULL;


#if 0
  if (ps && ps->state!=s_ok) {
    return 0;
  }
#endif

  if ((sz_in=pay_svr_http_rx(net,pconn))>0) {

    backend_entry_t bentry = get_backend_entry();
    backend_t be = get_backend(bentry,pconn->fd);
    connection_t peer = be->peer ;
    pay_data_t pd = be->pd ;

    b = dbuffer_ptr(pconn->rxb,0);
    b[sz_in] = '\0' ;

    // XXX: debug
    printf("%s",b);

    dbuffer_lseek(pconn->rxb,sz_in,SEEK_CUR,0);

    // reply to client
    do_ok(peer);
    peer->l4opt.tx(net,peer);

    // accumulate weight
    pd->weight ++ ;
  }

  return 0;
}

static
void alipay_ssl_outbound_release()
{
}

struct module_struct_s g_alipay_ssl_outbound_mod = {

  .name = "alipay outbound",

  .id = -1,

  .dyn_handle = NULL,

  .ssl = true,

  .opts[outbound_l5] = {
    .rx = alipay_ssl_outbound_rx,
    .tx = alipay_ssl_outbound_tx,
    .init = alipay_ssl_outbound_init,
    .release = alipay_ssl_outbound_release,
  },
};


static 
int do_alipay_order(Network_t net,connection_t pconn,
                    pay_data_t pd,tree_map_t postParams)
{
  int fd = 0;
  connection_t out_conn = pconn ;
  char host[128]="", *str = 0, *url = 0;
  int port = 443 ;
  unsigned long addr = 0L;
  tree_map_t pay_params = pd->pay_params ;
  tree_map_t pay_data  = NULL ;
  int param_type = 0; 
  backend_entry_t bentry = get_backend_entry();


  url = get_tree_map_value(pay_params,REQ_URL,strlen(REQ_URL));
  str = get_tree_map_value(pay_params,REQ_PORT,strlen(REQ_PORT));

  // connect to remote host
  if (url) {

    if (str)
      port = atoi(str) ;

    parse_http_url(url,host,128,NULL,0L);
    addr = hostname_to_uladdr(host) ;

    // connect to server 
    fd = new_tcp_client(addr,port);

    if (fd<0) {
      return -1;
    }

    out_conn = net->reg_outbound(net,fd,g_alipay_ssl_outbound_mod.id);

    // save backend info
    create_backend(bentry,fd,pconn,pd);
  }

  str  = get_tree_map_value(pay_params,PARAM_TYPE,strlen(PARAM_TYPE));
  if (str) {
    param_type = !strcmp(str,"html")?pt_html:pt_json;
  }

  // construct pay request
  pay_data = get_tree_map_nest(pay_params,PAY_DATA,strlen(PAY_DATA));

  if (create_http_post_req(&out_conn->txb,url,param_type,pay_data)) {
    return -1;
  }

  if (!out_conn->ssl || out_conn->ssl->state==s_ok) {
    alipay_ssl_outbound_tx(net,out_conn);
  }

  return 0;
}

static
struct pay_action_s action__alipay_order = 
{
  .key = "alipay/payApi/pay",
  .channel = "alipay",
  .action  = "payApi/pay",
  .cb      = do_alipay_order,
} ;


static
int alipay_ssl_outbound_init(Network_t net)
{
  pay_action_entry_t pe = get_pay_action_entry();
  pay_action_t pa = &action__alipay_order;
  

  add_pay_action(pe,pa);

  log_info("registering '%s: %s'...\n",pa->channel,pa->action);
  return 0;
}

