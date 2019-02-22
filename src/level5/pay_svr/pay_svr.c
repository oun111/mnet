#include <string.h>
#include "instance.h"
#include "action.h"
#include "config.h"
#include "http_svr.h"
#include "backend.h"
#include "order.h"
#include "merchant.h"
#include "myredis.h"
#include "module.h"
#include "log.h"




static
struct pay_svr_data {

  char conf_path[PATH_MAX];

  struct paySvr_config_s m_conf;

  struct backend_entry_s m_backends;

  pay_channels_entry_t m_paych ;

  struct order_entry_s m_orders ;

  struct merchant_entry_s m_merchant ;

  struct myredis_s m_rds ;

} g_paySvrData ;



paySvr_config_t get_running_configs()
{
  return &g_paySvrData.m_conf;
}

backend_entry_t get_backend_entry()
{
  return &g_paySvrData.m_backends;
}

pay_channels_entry_t get_pay_channels_entry()
{
  return g_paySvrData.m_paych ;
}

order_entry_t get_order_entry()
{
  return &g_paySvrData.m_orders ;
}

merchant_entry_t get_merchant_entry()
{
  return &g_paySvrData.m_merchant ;
}

myredis_t get_myredis()
{
  return &g_paySvrData.m_rds ;
}

static
int parse_cmd_line(int argc, char *argv[])
{
  //char *ptr = 0;

  for (int i=1; i<argc; i++) {
    if (!strcmp(argv[i],"-cp") && i+1<argc) {
      strcpy(g_paySvrData.conf_path,argv[i+1]);
    }
    else if (!strcmp(argv[i],"-h")) {
      printf("help message\n");
      return 1;
    }
  }
  return 0;
}

static void register_extra_modules()
{
  extern struct module_struct_s g_alipay_mod ;

  register_module(&g_alipay_mod);
}

void pay_svr_module_init(int argc, char *argv[])
{
  char host[32] = "", name[32] = "" ;
  int port = 0, notify_port = 0, ret = 0;


  if (parse_cmd_line(argc,argv))
    return ;

  if (init_config(&g_paySvrData.m_conf,g_paySvrData.conf_path))
    return ;

  notify_port = get_notify_port(&g_paySvrData.m_conf);
  port        = get_listen_port(&g_paySvrData.m_conf);
  get_bind_address(&g_paySvrData.m_conf,host);

  // the http base entry
  __http_svr_entry(host,port,notify_port);

  init_backend_entry(&g_paySvrData.m_backends,-1);

  init_pay_data(&g_paySvrData.m_paych);

  init_order_entry(&g_paySvrData.m_orders,-1);

  init_merchant_data(&g_paySvrData.m_merchant);

  // redis params
  if (!get_myredis_info(&g_paySvrData.m_conf,host,&port,name)) {
    ret = myredis_init(&g_paySvrData.m_rds,host,port,name);
    log_info("connect to redis(%s:%d,name: %s) %s\n",
             host,port,name,ret?"fail":"ok");
  }

  register_extra_modules();

  // XXX: test
  {
    extern void test_myredis();
    test_myredis();
  }
}

void pay_svr_module_exit()
{
  release_all_orders(&g_paySvrData.m_orders);

  delete_pay_channels_entry(g_paySvrData.m_paych);

  release_all_backends(&g_paySvrData.m_backends);

  release_all_merchants(&g_paySvrData.m_merchant);

  myredis_release(&g_paySvrData.m_rds);

  __http_svr_exit();

  free_config(&g_paySvrData.m_conf);
}

