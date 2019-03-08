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
#include "myrbtree.h"
#include "rds_order.h"




static
struct pay_svr_data {

  char conf_path[PATH_MAX];

  struct paySvr_config_s m_conf;

  struct backend_entry_s m_backends;

  pay_channels_entry_t m_paych ;

  struct order_entry_s m_orders ;

  struct merchant_entry_s m_merchant ;

  struct myredis_s m_rds ;

  struct rds_order_entry_s m_rOrders ;

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

rds_order_entry_t get_rds_order_entry()
{
  return &g_paySvrData.m_rOrders;
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

int 
get_remote_configs(myredis_t rds, char *tbl, char *key, dbuffer_t *res)
{
  int rc = 1;


  *res = alloc_default_dbuffer();

  // FIXME:
  for (int i=0; rc==1/*&&i<5*/;i++) {
    rc = myredis_read(rds,tbl,key,res);
  }

  if (rc==-1) {
    log_error("read '%s' from redis fail\n",tbl);
    return -1;
  }

  log_info("read remote '%s' from redis ok!\n",tbl);
  return 0;
}

int init_config2(myredis_conf_t rconf)
{
  dbuffer_t chan_res = 0, mch_res = 0;
  int ret = 0;


  // try read configs from redis
  if (is_myredis_ok(&g_paySvrData.m_rds)==true) {
    mysql_conf_t mscfg = get_mysql_configs(&g_paySvrData.m_conf);


    log_info("try read configs from redis %s:%d - %s\n",
             rconf->host, rconf->port, rconf->cfg_cache);

    // channels'
    get_remote_configs(&g_paySvrData.m_rds,mscfg->alipay_conf_table,"",&chan_res);

    // merchants'
    get_remote_configs(&g_paySvrData.m_rds,mscfg->mch_conf_table,"",&mch_res);
  }
  else {
    log_info("connect to redis %s:%d - %s fail, try read local configs\n",
        rconf->host, rconf->port, rconf->cfg_cache);
  }

  if (process_channel_configs(&g_paySvrData.m_conf,chan_res) ||
      process_merchant_configs(&g_paySvrData.m_conf,mch_res)) {
    ret = -1;
    goto __done;
  }

  // channels
  init_pay_data(&g_paySvrData.m_paych);

  // merchant
  init_merchant_data(&g_paySvrData.m_merchant);

__done:
  drop_dbuffer(chan_res);
  drop_dbuffer(mch_res);

  return ret;
}

void pay_svr_module_init(int argc, char *argv[])
{
  char host[32] = "" ;
  int port = 0, notify_port = 0, ret = 0;
  struct myredis_config_s rconf ;


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
  if (!get_myredis_configs(&g_paySvrData.m_conf,&rconf)) {

    ret = myredis_init(&g_paySvrData.m_rds, rconf.host,
                       rconf.port, rconf.cfg_cache);
    log_info("connect to redis %s:%d ... %s\n",
             rconf.host, rconf.port, ret?"fail!":"ok!");
  }

  if (init_config2(&rconf)) {
    return ;
  }

  init_backend_entry(&g_paySvrData.m_backends,-1);

  init_order_entry(&g_paySvrData.m_orders,-1);

  init_rds_order_entry(&g_paySvrData.m_rOrders,g_paySvrData.m_rds.ctx,
                       rconf.order_cache);

  register_extra_modules();
}

void pay_svr_module_exit()
{
  release_all_rds_orders(&g_paySvrData.m_rOrders);

  release_all_orders(&g_paySvrData.m_orders);

  delete_pay_channels_entry(g_paySvrData.m_paych);

  release_all_backends(&g_paySvrData.m_backends);

  release_all_merchants(&g_paySvrData.m_merchant);

  myredis_release(&g_paySvrData.m_rds);

  __http_svr_exit();

  free_config(&g_paySvrData.m_conf);
}

