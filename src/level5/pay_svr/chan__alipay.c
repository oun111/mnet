
#include <time.h>
#include <string.h>
#include "pay_action.h"
#include "log.h"


static const char normalHdr[] = "HTTP/1.1 %s\r\n"
                         "Server: pay-svr/0.1\r\n"
                         //"Content-Type: text/html;charset=GBK\r\n"
                         "Content-Type: text/json;charset=GBK\r\n"
                         "Content-Length:%zu      \r\n"
                         "Date: %s\r\n\r\n";

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

static int do_alipay_order(connection_t pconn,tree_map_t params)
{
  log_info("invoked!!\n");
  do_ok(pconn);
  return 0;
}


struct pay_action_s action__alipay_order = 
{
  .key = "alipay/payApi/pay",
  .channel = "alipay",
  .action  = "payApi/pay",
  .cb      = do_alipay_order,
} ;


