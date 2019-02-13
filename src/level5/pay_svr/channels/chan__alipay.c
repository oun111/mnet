
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
#include "chan.h"


static const char normalHdr[] = "HTTP/1.1 %s\r\n"
                         "Server: pay-svr/0.1\r\n"
                         //"Content-Type: text/html;charset=GBK\r\n"
                         "Content-Type: text/json;charset=GBK\r\n"
                         "Content-Length:%zu      \r\n"
                         "Date: %s\r\n\r\n";



static int alipay_init(Network_t net);

static struct http_action_s action__alipay_order;


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
int alipay_tx(Network_t net, connection_t pconn)
{
  log_debug("tx: \n");
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
  tree_map_t crypto_map = get_tree_map_nest(pay_params,CRYPTO,strlen(CRYPTO));
  char *privkey = 0, *pubkey = 0;


  if (!crypto_map) {
    return 1;
  }

  pubkey = get_tree_map_value(crypto_map,PUBKEY,strlen(PUBKEY));
  privkey= get_tree_map_value(crypto_map,PRIVKEY,strlen(PRIVKEY));

  printf("public key: %s, private key: %s\n",pubkey,privkey);


  return 0;
}

static 
int do_alipay_order(Network_t net,connection_t pconn,
                    tree_map_t postParams)
{
  int fd = 0;
  connection_t out_conn = pconn ;
  char host[128]="", *str = 0, *url = 0;
  unsigned long addr = 0L;
  tree_map_t pay_data  = NULL ;
  int param_type = 0; 
  const char *payChan = action__alipay_order.channel ;
  pay_data_t pd = get_pay_route(get_pay_channels_entry(),payChan);
  tree_map_t pay_params = NULL ;


  if (!pd) {
    return -1;
  }

  pay_params = pd->pay_params ;

  url = get_tree_map_value(pay_params,REQ_URL,strlen(REQ_URL));

  // connect to remote host
  if (url) {

    bool is_ssl = true ;
    int port = -1 ;


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
      return -1;
    }

    out_conn = net->reg_outbound(net,fd,g_alipay_mod.id);

    // save backend info
    create_backend(get_backend_entry(),fd,pconn,pd);
  }

  str  = get_tree_map_value(pay_params,PARAM_TYPE,strlen(PARAM_TYPE));
  if (str) {
    param_type = !strcmp(str,"html")?pt_html:pt_json;
  }

  // deals with cryptographic
  deal_crypto(pay_params);

  // construct pay request
  pay_data = get_tree_map_nest(pay_params,PAY_DATA,strlen(PAY_DATA));

  if (create_http_post_req(&out_conn->txb,url,param_type,pay_data)) {
    return -1;
  }

  if (!out_conn->ssl || out_conn->ssl->state==s_ok) {
    alipay_tx(net,out_conn);
  }

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
int alipay_init(Network_t net)
{
  http_action_entry_t pe = get_http_action_entry();
  http_action_t pa = &action__alipay_order;
  

  add_http_action(pe,pa);

  log_info("registering '%s: %s'...\n",pa->channel,pa->action);
  return 0;
}

