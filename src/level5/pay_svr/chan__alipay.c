
#include <time.h>
#include <string.h>
#include "pay_action.h"
#include "log.h"
#include "instance.h"
#include "module.h"
#include "socket.h"
#include "ssl_state.h"


static const char normalHdr[] = "HTTP/1.1 %s\r\n"
                         "Server: pay-svr/0.1\r\n"
                         //"Content-Type: text/html;charset=GBK\r\n"
                         "Content-Type: text/json;charset=GBK\r\n"
                         "Content-Length:%zu      \r\n"
                         "Date: %s\r\n\r\n";

static struct sslstate_entry_s g_ssl_states;


static int do_alipay_order(Network_t,connection_t,tree_map_t);

static void register_alipay_modules();


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


struct pay_action_s action__alipay_order = 
{
  .key = "alipay/payApi/pay",
  .channel = "alipay",
  .action  = "payApi/pay",
  .cb      = do_alipay_order,
  .reg_modules = register_alipay_modules,
} ;


static
int alipay_outbound_tx(Network_t net, connection_t pconn)
{
  pconn->l4opt.tx(net,pconn);
  return 0;
}

static
int alipay_outbound_rx(Network_t net, connection_t pconn)
{
  size_t sz_in = dbuffer_data_size(pconn->rxb);
  dbuffer_t b = dbuffer_ptr(pconn->rxb,0);


  if (sz_in>0) {
    b[sz_in] = '\0' ;

    log_info("rx111111: %s\n",b);

    dbuffer_lseek(pconn->rxb,sz_in,SEEK_CUR,0);
  }

  return 0;
}

static
int alipay_outbound_init(Network_t net)
{
  init_sslstate_entry(&g_ssl_states);
  log_info("done\n");
  return 0;
}

static
void alipay_outbound_release()
{
}

static 
struct module_struct_s g_alipay_ssl_outbound_mod = {

  .name = "alipay outbound",

  .id = -1,

  .dyn_handle = NULL,

  .ssl = true,

  .opts[outbound_l5] = {
    .rx = alipay_outbound_rx,
    .tx = alipay_outbound_tx,
    .init = alipay_outbound_init,
    .release = alipay_outbound_release,
  },
};


static void register_alipay_modules()
{
  register_module(&g_alipay_ssl_outbound_mod);
}

static int do_alipay_order(Network_t net,connection_t pconn,tree_map_t params)
{
  int fd = 0, ret = 0;
  connection_t out_conn = NULL ;
  const char *host = "www.163.com"; // FIXME
  const int port = 443 ;
  unsigned long addr = hostname_to_uladdr(host);


  fd = new_tcp_socket();

  out_conn = net->reg_outbound(net,fd,g_alipay_ssl_outbound_mod.id);

  create_sslstate(&g_ssl_states,fd,out_conn);

  new_tcp_client2(fd,addr,port);

  ret = ssl_connect(out_conn->ssl,fd);

  if (ret==1) {
    char req[] = "GET / \r\n\r\n";
    const size_t sz = strlen(req);


    pconn->txb = write_dbuffer(out_conn->txb,req,sz);

    alipay_outbound_tx(net,out_conn);
  }
  else {
    update_sslstate_st(&g_ssl_states,fd,s_connecting);

  }

  return 0;
}

