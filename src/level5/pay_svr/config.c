#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "file_cache.h"
#include "mm_porting.h"
#include "log.h"
#include "myrbtree.h"
#include "socket.h"


static const  
struct global_config_keywords {
  const char *gsSec;
  const char *bindAddr;
  const char *listenPort;
  const char *notifyPort;
  const char *maxCachedOdrs;
  const char *channels;
  const char *merchants;
  const char *redis;
  const char *mysql;
  const char *address;
  const char *port;
  const char *cfgCache;
  const char *odrCache;
  const char *rcCache;
  const char *cfgScanIntv;
  const char *alipayCfgTbl ;
  const char *mchCfgTbl ;
  const char *rcCfgTbl ;
  const char *odrTbl ;
  const char *riskCtl ;
} 
g_confKW = {
  .gsSec        = "Globals",
  .bindAddr     = "BindAddress",
  .listenPort   = "ListenPort",
  .notifyPort   = "NotifyPort",
  .maxCachedOdrs= "MaxCachedOrders",
  .channels     = "channels",
  .merchants    = "merchants",
  .redis        = "Redis",
  .mysql        = "Mysql",
  .address      = "address",
  .port         = "port",
  .cfgCache     = "cfgCache",
  .odrCache     = "orderCache",
  .rcCache      = "rcCache",
  .cfgScanIntv  = "cfgScanInterval",
  .alipayCfgTbl = "alipayConfigTableName",
  .mchCfgTbl    = "merchantConfigTableName",
  .odrTbl       = "orderTableName",
  .rcCfgTbl     = "rcTableName",
  .riskCtl      = "riskControl",
} ;


int get_bind_address(paySvr_config_t conf, char *host)
{
  char *pv = 0;
  size_t vl= 0L ;
  jsonKV_t *pg = jsons_find(conf->m_root,g_confKW.gsSec), *p= 0;


  if (!pg) {
    log_error("entry '%s' not found\n",g_confKW.gsSec);
    return -1;
  }

  p = jsons_find(pg,g_confKW.bindAddr);
  if (!p) {
    log_error("entry '%s' not found\n",g_confKW.bindAddr);
    return -1;
  }

  pv = jsons_string(p->value,&vl);
  strncpy(host,pv,vl);

  return 0;
}

int get_listen_port(paySvr_config_t conf)
{
  jsonKV_t *pg = jsons_find(conf->m_root,g_confKW.gsSec), *p= 0;


  if (!pg) {
    log_error("entry '%s' not found\n",g_confKW.gsSec);
    return -1;
  }

  p = jsons_find(pg,g_confKW.listenPort);
  if (!p) {
    log_error("entry '%s' not found\n",g_confKW.listenPort);
    return -1;
  }

  return jsons_integer(p->value);
}

int get_notify_port(paySvr_config_t conf)
{
  jsonKV_t *pg = jsons_find(conf->m_root,g_confKW.gsSec), *p= 0;


  if (!pg) {
    log_error("entry '%s' not found\n",g_confKW.gsSec);
    return -1;
  }

  p = jsons_find(pg,g_confKW.notifyPort);
  if (!p) {
    log_error("entry '%s' not found\n",g_confKW.notifyPort);
    return -1;
  }

  return jsons_integer(p->value);
}

int get_max_cached_orders(paySvr_config_t conf)
{
  jsonKV_t *pg = jsons_find(conf->m_root,g_confKW.gsSec), *p= 0;


  if (!pg) {
    log_error("entry '%s' not found\n",g_confKW.gsSec);
    return -1;
  }

  p = jsons_find(pg,g_confKW.maxCachedOdrs);
  if (!p) {
    log_error("entry '%s' not found\n",g_confKW.notifyPort);
    return -1;
  }

  return jsons_integer(p->value);
}

static
int get_conf_str(jsonKV_t *pr, const char *key, char *buf, size_t szbuf)
{
  char *pstr = (char*)key ;
  jsonKV_t *p = jsons_find(pr,pstr);
  size_t vl = 0L;


  if (!p) {
    log_error("entry '%s' not found\n",pstr);
    return -1;
  }

  pstr = jsons_string(p->value,&vl);
  vl   = vl<szbuf?vl:szbuf;
  strncpy(buf,pstr,vl);
  buf[vl] = '\0';

  return 0;
}

static
int get_conf_int(jsonKV_t *pr, const char *key, int *val)
{
  char *pstr = (char*)key ;
  jsonKV_t *p = jsons_find(pr,pstr);


  if (!p) {
    log_error("entry '%s' not found\n",pstr);
    return -1;
  }

  *val = jsons_integer(p->value);

  return 0;
}

myredis_conf_t get_myredis_configs(paySvr_config_t conf)
{
  return &conf->myrds_conf;
}

mysql_conf_t get_mysql_configs(paySvr_config_t conf)
{
  return &conf->mysql_conf;
}

int process_myredis_configs(paySvr_config_t conf)
{
  char *pstr = (char*)g_confKW.redis ;
  jsonKV_t *pr = jsons_find(conf->m_root,pstr);
  myredis_conf_t pcfg = &conf->myrds_conf ;


  if (!pr) {
    log_error("entry '%s' not found\n",pstr);
    return -1;
  }

  get_conf_str(pr,g_confKW.address,pcfg->host,sizeof(pcfg->host));

  get_conf_int(pr,g_confKW.port,&pcfg->port);

  get_conf_str(pr,g_confKW.cfgCache,pcfg->cfg_cache,sizeof(pcfg->cfg_cache));

  get_conf_str(pr,g_confKW.odrCache,pcfg->order_cache,sizeof(pcfg->order_cache));

  get_conf_str(pr,g_confKW.rcCache,pcfg->rc_cache,sizeof(pcfg->rc_cache));

  get_conf_int(pr,g_confKW.cfgScanIntv,&pcfg->cfg_scan_interval);

  return 0;
}

static
int do_process_configs(paySvr_config_t conf, tree_map_t *target_conf, 
                       const char *key, dbuffer_t cfgStr)
{
  // channels configs
  jsonKV_t *cc = 0, *pr = 0;


  // FIXME: check json validations
  if (cfgStr && dbuffer_data_size(cfgStr)>0) {
    pr = jsons_parse(cfgStr);
    //jsons_dump(pr);
  }
  else {
    pr = conf->m_root;
  }
  cc = jsons_find(pr,key);

  if (!cc) {
    log_error("no '%s' configs\n",key);
    return -1;
  }

  if (*target_conf)
    delete_tree_map(*target_conf);

  *target_conf = jsons_to_treemap(cc) ;

  if (cfgStr)
    jsons_release(pr);

  return 0;
}

int process_channel_configs(paySvr_config_t conf, dbuffer_t chanCfgStr)
{
  return do_process_configs(conf,&conf->chan_conf,g_confKW.channels,chanCfgStr);
}

int process_merchant_configs(paySvr_config_t conf, dbuffer_t mchCfgStr)
{
  return do_process_configs(conf,&conf->mch_conf,g_confKW.merchants,mchCfgStr);
}

int process_rc_configs(paySvr_config_t conf, dbuffer_t rcCfgStr)
{
  return do_process_configs(conf,&conf->rc_conf,g_confKW.riskCtl,rcCfgStr);
}

int process_mysql_configs(paySvr_config_t conf)
{
  char *pstr = (char*)g_confKW.mysql ;
  jsonKV_t *pr = jsons_find(conf->m_root,pstr);
  mysql_conf_t pcfg = &conf->mysql_conf;


  if (!pr) {
    log_error("entry '%s' not found\n",pstr);
    return -1;
  }

  get_conf_str(pr,g_confKW.alipayCfgTbl,pcfg->alipay_conf_table,
               sizeof(pcfg->alipay_conf_table));

  get_conf_str(pr,g_confKW.mchCfgTbl,pcfg->mch_conf_table,
               sizeof(pcfg->mch_conf_table));

  get_conf_str(pr,g_confKW.odrTbl,pcfg->order_table,
               sizeof(pcfg->order_table));

  get_conf_str(pr,g_confKW.rcCfgTbl,pcfg->rc_conf_table,
               sizeof(pcfg->rc_conf_table));

  return 0;
}

tree_map_t get_rc_conf_by_channel(paySvr_config_t conf, const char *chan)
{
  tm_item_t pos,n;
  tree_map_t rc_cfg = get_tree_map_nest(conf->rc_conf,(char*)g_confKW.riskCtl);

  if (!rc_cfg)
    return NULL ;

  //rbtree_postorder_for_each_entry_safe(pos,n,&rc_cfg->u.root,node) {
  MY_RBTREE_PREORDER_FOR_EACH_ENTRY_SAFE(pos,n,&rc_cfg->u.root,node) {
    tree_map_t rc_map = pos->nest_map;
    const char *rcname = get_tree_map_value(rc_map,"channel") ;

    //dump_tree_map(rc_map);
    if (rcname && !strcasecmp(chan,rcname))
      return rc_map ;
  }

  return NULL;
}

tree_map_t get_rc_conf_by_rcid(paySvr_config_t conf, const char *rcid)
{
  tm_item_t pos,n;
  tree_map_t rc_cfg = get_tree_map_nest(conf->rc_conf,(char*)g_confKW.riskCtl);

  if (!rc_cfg)
    return NULL ;

  //rbtree_postorder_for_each_entry_safe(pos,n,&rc_cfg->u.root,node) {
  MY_RBTREE_PREORDER_FOR_EACH_ENTRY_SAFE(pos,n,&rc_cfg->u.root,node) {
    tree_map_t rc_map = pos->nest_map;
    const char *rcname = get_tree_map_value(rc_map,"rcid") ;

    //dump_tree_map(rc_map);
    if (rcname && !strcasecmp(rcid,rcname))
      return rc_map ;
  }

  return NULL;
}

int init_config(paySvr_config_t conf, const char *infile)
{
  dbuffer_t content = NULL ;
  int err = 0;


  if (load_file(infile,&content) || !(conf->m_root=jsons_parse(content))) 
    err = -1;

  drop_dbuffer(content);
  
  if (err)
    return -1;

  conf->chan_conf = NULL;
  conf->mch_conf  = NULL;
  conf->rc_conf   = NULL;

  process_mysql_configs(conf);

  process_myredis_configs(conf);

  //process_risk_control_configs(conf);

  return 0;
}

int free_config(paySvr_config_t conf)
{
  jsons_release(conf->m_root);

  if (conf->chan_conf) {
    delete_tree_map(conf->chan_conf);
  }

  if (conf->mch_conf) {
    delete_tree_map(conf->mch_conf);
  }

  if (conf->rc_conf) {
    delete_tree_map(conf->rc_conf);
  }

  return 0;
}

