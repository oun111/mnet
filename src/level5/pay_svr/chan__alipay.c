
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
  pconn->l4opt.tx(net,pconn);
  return 0;
}

static
int alipay_ssl_outbound_rx(Network_t net, connection_t pconn)
{
  size_t sz_in = 0L;
  dbuffer_t b = NULL;
  connection_t peer = NULL;


#if 0
  if (ps && ps->state!=s_ok) {
    return 0;
  }
#endif

  if ((sz_in=pay_svr_http_rx(net,pconn))>0) {

    b = dbuffer_ptr(pconn->rxb,0);
    b[sz_in] = '\0' ;

    printf("%s",b);

    dbuffer_lseek(pconn->rxb,sz_in,SEEK_CUR,0);

    peer = pconn->ssl->peer ;
    do_ok(peer);
    peer->l4opt.tx(net,peer);
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


static int do_alipay_order(Network_t net,connection_t pconn,tree_map_t params)
{
  int fd = 0;
  connection_t out_conn = NULL ;
  const char *host = "www.163.com"; // FIXME
  const int port = 443 ;
  unsigned long addr = hostname_to_uladdr(host);


  // connect to server first
  fd = new_tcp_client(addr,port);

  out_conn = net->reg_outbound(net,fd,g_alipay_ssl_outbound_mod.id);

  out_conn->ssl->peer = pconn ;

  {
    char req[] = "GET / \r\n\r\n";
    const size_t sz = strlen(req);


    out_conn->txb = write_dbuffer(out_conn->txb,req,sz);

    if (out_conn->ssl->state==s_ok)
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
  // FIXME: 
  register_pay_action(&action__alipay_order);

  log_info("done!\n");
  return 0;
}

