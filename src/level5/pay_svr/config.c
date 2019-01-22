#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "file_cache.h"
#include "mm_porting.h"
#include "log.h"
#include "myrbtree.h"
#include "socket.h"


#define CHECK_DUPLICATE(list,item,member) ({ \
  int dup = 0;   \
  jsonKV_t *p = 0;  \
  list_for_each_entry(p,(list),member) \
    if (p!=item && !strcmp(p->key,item->key)) {\
      dup = 1; \
      break; \
    } \
  dup ;\
})

static const  
struct global_config_keywords {
  const char *gsSec;
  const char *bindAddr;
  const char *listenPort;
  const char *notifyPort;
  const char *channels;
  const char *merchant;
  const char *url;
  const char *apikey;
  const char *privkey;
  const char *pubkey;
} 
g_confKW = {
  .gsSec        = "Globals",
  .bindAddr     = "BindAddress",
  .listenPort   = "ListenPort",
  .notifyPort   = "NotifyPort",
  .channels     = "channels",
  .merchant     = "merchant",
  .url          = "url",
  .apikey       = "apikey",
  .privkey      = "privkey",
  .pubkey       = "pubkey",
} ;



static 
int parse_global_settings(paySvr_config_t conf)
{
  size_t vl= 0L ;
  char *pv = NULL;
  jsonKV_t *pg = jsons_find(conf->m_root,g_confKW.gsSec), *p= 0;
  global_setting_t ps = &conf->m_globSettings ;


  if (!pg) {
    log_error("entry '%s' not found\n",g_confKW.gsSec);
    return -1;
  }

  bzero(ps,sizeof(*ps));

  p = jsons_find(pg,g_confKW.bindAddr);
  if (p) {
    pv = jsons_string(p->value,&vl);
    strncpy(ps->bindAddr,pv,vl);
  }

  p = jsons_find(pg,g_confKW.listenPort);
  if (p) 
    ps->listenPort = jsons_integer(p->value);

  p = jsons_find(pg,g_confKW.notifyPort);
  if (p) 
    ps->notifyPort = jsons_integer(p->value);

  return 0;
}

const char* get_bind_address(paySvr_config_t conf)
{
  return conf->m_globSettings.bindAddr ;
}

int get_listen_port(paySvr_config_t conf)
{
  return conf->m_globSettings.listenPort ;
}

int get_notify_port(paySvr_config_t conf)
{
  return conf->m_globSettings.notifyPort ;
}

channel_config_t new_channel_config() 
{
  channel_config_t cc = kmalloc(sizeof(struct channel_config_s),0L);

  cc->url    = alloc_default_dbuffer();
  cc->apikey = alloc_default_dbuffer();
  cc->pubkey = alloc_default_dbuffer();
  cc->privkey= alloc_default_dbuffer();

  INIT_LIST_HEAD(&cc->upper);

  return cc ;
}

void free_channel_config(channel_config_t cc)
{
  drop_dbuffer(cc->url);
  drop_dbuffer(cc->apikey);
  drop_dbuffer(cc->pubkey);
  drop_dbuffer(cc->privkey);

  kfree(cc);
}

static 
int parse_channels(paySvr_config_t conf)
{
  size_t vl= 0L ;
  char *pv = NULL;
  jsonKV_t *pc = jsons_find(conf->m_root,g_confKW.channels), *p= 0, *ptr=0;


  if (!pc) {
    return 0;
  }

  list_for_each_entry(p,&pc->children,upper) {

    channel_config_t cc = new_channel_config();

    // type
    strncpy(cc->type,p->key,sizeof(cc->type));

    // url
    ptr = jsons_find(p,g_confKW.url);
    if (ptr) {
      pv = jsons_string(ptr->value,&vl);
      cc->url = write_dbuffer(cc->url,pv,vl);
    }

    // merchant
    ptr = jsons_find(p,g_confKW.merchant);
    if (ptr) {
      pv = jsons_string(ptr->value,&vl);
      strncpy(cc->merchant,pv,vl);
    }

    // apikey
    ptr = jsons_find(p,g_confKW.apikey);
    if (ptr) {
      pv = jsons_string(ptr->value,&vl);
      cc->apikey = write_dbuffer(cc->apikey,pv,vl);
    }

    // public key
    ptr = jsons_find(p,g_confKW.pubkey);
    if (ptr) {
      pv = jsons_string(ptr->value,&vl);
      cc->pubkey = write_dbuffer(cc->pubkey,pv,vl);
    }

    // private key
    ptr = jsons_find(p,g_confKW.privkey);
    if (ptr) {
      pv = jsons_string(ptr->value,&vl);
      cc->privkey = write_dbuffer(cc->privkey,pv,vl);
    }

    list_add(&cc->upper,&conf->m_channels);
  }

  return 0;
}

channel_config_t get_channel_conf(paySvr_config_t conf,const char *type)
{
  channel_config_t pc = 0;

  list_for_each_entry(pc,&conf->m_channels,upper) {
    if (!strcmp(pc->type,type))
      return pc;
  }

  return NULL;
}

static
int parse_content(paySvr_config_t conf)
{
  /* parse global settings */
  if (parse_global_settings(conf)) {
    return -1;
  }

  if (parse_channels(conf)) {
    return -1;
  }

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

  INIT_LIST_HEAD(&conf->m_channels); 

  if (parse_content(conf)) {
    log_error("parse config content fail\n");
    return -1;
  }

  return 0;
}

int free_config(paySvr_config_t conf)
{
  channel_config_t pc,n;


  jsons_release(conf->m_root);

  list_for_each_entry_safe(pc,n,&conf->m_channels,upper) {
    free_channel_config(pc);
  }

  return 0;
}

