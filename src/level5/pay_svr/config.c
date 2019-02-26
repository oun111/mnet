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
  const char *channels;
  const char *merchants;
  const char *redis;
  const char *address;
  const char *port;
  const char *dataTbl;
  const char *cfgTbl;
} 
g_confKW = {
  .gsSec        = "Globals",
  .bindAddr     = "BindAddress",
  .listenPort   = "ListenPort",
  .notifyPort   = "NotifyPort",
  .channels     = "channels",
  .merchants    = "merchants",
  .redis        = "Redis",
  .address      = "address",
  .port         = "port",
  .dataTbl      = "dataTable",
  .cfgTbl       = "configTable",
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

int 
get_myredis_info(paySvr_config_t conf, char *host, int *port, 
                 char *dataTbl, char *cfgTbl)
{
  size_t vl = 0L;
  char *pstr = (char*)g_confKW.redis ;
  jsonKV_t *pr = jsons_find(conf->m_root,pstr), *p= 0;


  if (!pr) {
    log_error("entry '%s' not found\n",pstr);
    return -1;
  }

  pstr = (char*)g_confKW.address ;
  p = jsons_find(pr,pstr);
  if (!p) {
    log_error("entry '%s' not found\n",pstr);
    return -1;
  }

  pstr = jsons_string(p->value,&vl);
  strncpy(host,pstr,vl);

  pstr = (char*)g_confKW.port ;
  p = jsons_find(pr,pstr);
  if (!p) {
    log_error("entry '%s' not found\n",pstr);
    return -1;
  }

  *port = jsons_integer(p->value);

  pstr = (char*)g_confKW.dataTbl ;
  p = jsons_find(pr,pstr);
  if (!p) {
    log_error("entry '%s' not found\n",pstr);
    return -1;
  }

  pstr = jsons_string(p->value,&vl);
  strncpy(dataTbl,pstr,vl);


  pstr = (char*)g_confKW.cfgTbl ;
  p = jsons_find(pr,pstr);
  if (!p) {
    log_error("entry '%s' not found\n",pstr);
    return -1;
  }

  pstr = jsons_string(p->value,&vl);
  strncpy(cfgTbl,pstr,vl);

  return 0;
}

int process_channel_configs(paySvr_config_t conf, dbuffer_t chanCfgStr)
{
  // channels configs
  jsonKV_t *cc = 0;


  // FIXME: check json validations
  if (chanCfgStr && dbuffer_data_size(chanCfgStr)>0) {
    cc = jsons_parse(chanCfgStr);
  }
  else {
    cc = jsons_find(conf->m_root,g_confKW.channels);
  }

  if (!cc) {
    log_error("no channel configs\n");
    return -1;
  }

  tree_map_t tm = jsons_to_treemap(cc);
  tree_map_t tm_chan = get_tree_map_nest(tm,(char*)g_confKW.channels);

  if (conf->chan_root) {
    delete_tree_map(conf->chan_root);
  }
  conf->chan_root = tm ;

  if (tm_chan) {
    conf->chan_cfg = tm_chan;
  }

  if (chanCfgStr)
    jsons_release(cc);

  return 0;
}

int process_merchant_configs(paySvr_config_t conf, dbuffer_t mchCfgStr)
{
  // merchants configs
  jsonKV_t *cc = 0;


  if (mchCfgStr && dbuffer_data_size(mchCfgStr)>0) {
    cc = jsons_parse(mchCfgStr);
  }
  else {
    cc = jsons_find(conf->m_root,g_confKW.merchants);
  }

  if (!cc) {
    log_error("no merchant configs\n");
    return -1;
  }

  tree_map_t tm     = jsons_to_treemap(cc);
  tree_map_t tm_mch = get_tree_map_nest(tm,(char*)g_confKW.merchants);

  if (conf->mch_root) {
    delete_tree_map(conf->mch_root);
  }
  conf->mch_root = tm ;

  if (tm_mch) {
    conf->merchant_cfg = tm_mch;
  }

  if (mchCfgStr)
    jsons_release(cc);

  return 0;
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

  return 0;
}

int free_config(paySvr_config_t conf)
{
  jsons_release(conf->m_root);

  if (conf->chan_root) {
    delete_tree_map(conf->chan_root);
  }

  if (conf->mch_root) {
    delete_tree_map(conf->mch_root);
  }

  return 0;
}

