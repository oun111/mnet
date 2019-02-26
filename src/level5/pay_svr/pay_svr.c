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
#include "myredis.h"




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

int init_config2(char *host, int port, char *rds_cfg_tbl)
{
  struct myredis_s rds = {.ctx=NULL,} ;
  dbuffer_t chan_res = 0, mch_res = 0;


  if (!myredis_init(&rds,host,port,rds_cfg_tbl)) {

    chan_res = alloc_default_dbuffer();
    if (!myredis_read(&rds,"channels","config",&chan_res,true)) {
    }

    mch_res = alloc_default_dbuffer();
    if (!myredis_read(&rds,"merchants","config",&mch_res,true)) {
    }
  }

  // channels
  process_local_channel_configs(&g_paySvrData.m_conf,chan_res);
  init_pay_data(&g_paySvrData.m_paych);

  // merchant
  process_local_merchant_configs(&g_paySvrData.m_conf,mch_res);
  init_merchant_data(&g_paySvrData.m_merchant);


  myredis_release(&rds);
  drop_dbuffer(chan_res);
  drop_dbuffer(mch_res);

  return 0;
}

void pay_svr_module_init(int argc, char *argv[])
{
  char host[32] = "", dataTbl[32] = "", cfgTbl[32] = "" ;
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

  // redis params
  if (!get_myredis_info(&g_paySvrData.m_conf,host,&port,dataTbl,cfgTbl)) {

    ret = myredis_init(&g_paySvrData.m_rds,host,port,dataTbl);
    log_info("connect to redis %s:%d - %s ... \n",host,port,dataTbl);
    if (ret) log_error("fail!\n");
    else log_info("ok!\n");
  }

  init_backend_entry(&g_paySvrData.m_backends,-1);

  init_order_entry(&g_paySvrData.m_orders,-1);

  init_config2(host,port,cfgTbl);

  register_extra_modules();
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

