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

static int conv_merchant_format(dbuffer_t inb, dbuffer_t *outb)
{
  tm_item_t pos,n;
  tree_map_t top  = new_tree_map();
  tree_map_t m0  = new_tree_map();

  jsonKV_t *p_in = jsons_parse(inb);
  tree_map_t entry = jsons_to_treemap(p_in);
  tree_map_t r0 = get_tree_map_nest(entry,"root");


  MY_RBTREE_PREORDER_FOR_EACH_ENTRY_SAFE(pos,n,&r0->u.root,node) {
    tree_map_t sub = pos->nest_map;
    tree_map_t m   = new_tree_map();
    char *signtype = get_tree_map_value(sub,"SIGN_TYPE");
    char *mch_id   = get_tree_map_value(sub,"NAME");

    put_tree_map_string(m,"sign_type",signtype);
    put_tree_map_string(m,"param_type",get_tree_map_value(sub,"PARAM_TYPE"));
    if (!strcmp(signtype,"md5"))
      put_tree_map_string(m,"key",get_tree_map_value(sub,"PUBKEY"));
    else if (!strcmp(signtype,"rsa")) {
      put_tree_map_string(m,"pubkey",get_tree_map_value(sub,"PUBKEY"));
      put_tree_map_string(m,"privkey",get_tree_map_value(sub,"PRIVKEY"));
    }

    put_tree_map_nest(m0,mch_id,strlen(mch_id),m);
  }

  put_tree_map_nest(top,"merchants",strlen("merchants"),m0);
  m0 = get_tree_map_nest(top,"merchants");

  treemap_to_jsons_str(top,outb);
  delete_tree_map(top);
  //delete_tree_map(m0);

  delete_tree_map(entry);
  jsons_release(p_in);

  return 0;
}

static int conv_channel_format(tree_map_t map, dbuffer_t *inb)
{
  return 0;
}

int init_config2(myredis_conf_t rconf)
{
  struct myredis_s rds = {.ctx=NULL,} ;
  dbuffer_t chan_res = 0, mch_res = 0;
  int ret = 0;


  // try read configs from redis
  if (!myredis_init(&rds,rconf->host,rconf->port,rconf->conf_table)) {
    int rc = 0;
    struct mysql_config_s mscfg ;


    get_mysql_configs(&g_paySvrData.m_conf,&mscfg);
    log_info("try read configs from redis %s:%d - %s\n",
             rconf->host,rconf->port,rconf->conf_table);

    // channels'
    rc = 1;
    chan_res = alloc_default_dbuffer();
    for (int i=0; rc==1&&i<5;i++) 
      rc = myredis_read(&rds,mscfg.chan_config_table,"",&chan_res);

    if (rc==-1) 
      log_error("read channels configs from redis fail\n");

    // merchants'
    rc = 1;
    mch_res = alloc_default_dbuffer();
    for (int i=0; rc==1&&i<5;i++) 
      rc = myredis_read(&rds,mscfg.mch_config_table,"",&mch_res);

    if (rc==-1) 
      log_error("read merchants configs from redis fail\n");
    else
      conv_merchant_format(mch_res,&mch_res);

  }
  else {
    log_info("connect to redis %s:%d - %s fail, try read local "
             "configs\n",rconf->host,rconf->port,rconf->conf_table);
  }

  if (process_channel_configs(&g_paySvrData.m_conf,/*chan_res*/NULL) ||
      process_merchant_configs(&g_paySvrData.m_conf,mch_res)) {
    ret = -1;
    goto __done;
  }

  // channels
  init_pay_data(&g_paySvrData.m_paych);

  // merchant
  init_merchant_data(&g_paySvrData.m_merchant);

__done:
  myredis_release(&rds);
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

    ret = myredis_init(&g_paySvrData.m_rds,rconf.host,
                       rconf.port,rconf.data_table);
    log_info("connect to redis %s:%d - %s ... %s\n",
             rconf.host,rconf.port,rconf.data_table,
             ret?"fail!":"ok!");
  }

  if (init_config2(&rconf)) {
    return ;
  }

  init_backend_entry(&g_paySvrData.m_backends,-1);

  init_order_entry(&g_paySvrData.m_orders,-1);

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

