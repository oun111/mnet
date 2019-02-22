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
  const char *name;
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
  .name         = "name",
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
get_myredis_info(paySvr_config_t conf, char *host, int *port, char *name)
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

  pstr = (char*)g_confKW.name ;
  p = jsons_find(pr,pstr);
  if (!p) {
    log_error("entry '%s' not found\n",pstr);
    return -1;
  }

  pstr = jsons_string(p->value,&vl);
  strncpy(name,pstr,vl);

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

  // channels configs
  jsonKV_t *cc = jsons_find(conf->m_root,g_confKW.channels);

  if (cc) {
    tree_map_t tm = jsons_to_treemap(cc);
    tree_map_t tm_chan = get_tree_map_nest(tm,(char*)g_confKW.channels);

    if (tm_chan) {
      conf->chan_cfg = tm_chan;
      //dump_tree_map(tm_chan);
    }
  }

  // merchants configs
  cc = jsons_find(conf->m_root,g_confKW.merchants);

  if (cc) {
    tree_map_t tm     = jsons_to_treemap(cc);
    tree_map_t tm_mch = get_tree_map_nest(tm,(char*)g_confKW.merchants);

    if (tm_mch) {
      conf->merchant_cfg = tm_mch;
      //dump_tree_map(tm_chan);
    }
  }

  return 0;
}

int free_config(paySvr_config_t conf)
{
  jsons_release(conf->m_root);

  if (conf->chan_cfg) {
    delete_tree_map(conf->chan_cfg);
  }

  if (conf->merchant_cfg) {
    delete_tree_map(conf->merchant_cfg);
  }

  return 0;
}

