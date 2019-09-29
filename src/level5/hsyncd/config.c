#include <stdlib.h>
#include <string.h>
#include <limits.h>
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
  const char *monitorPath;
} 
g_confKW = {
  .gsSec       = "Globals",
  .monitorPath = "monitorPath",
} ;



static 
int parse_global_settings(hsyncd_config_t conf)
{
  size_t vl = 0L;
  char *pv = NULL;
  jsonKV_t *pg = jsons_find(conf->m_root,g_confKW.gsSec), *p= 0;
  global_setting_t ps = &conf->m_globSettings ;


  if (!pg) {
    log_error("entry '%s' not found\n",g_confKW.gsSec);
    return -1;
  }

  bzero(ps,sizeof(*ps));

  p = jsons_find(pg,g_confKW.monitorPath);
  if (p) {
    pv = jsons_string(p->value,&vl);
    strncpy(ps->monitorPath,pv,vl);
  }

  return 0;
}

const char* get_monitor_path(hsyncd_config_t conf)
{
  return conf->m_globSettings.monitorPath ;
}

static
int parse_content(hsyncd_config_t conf)
{
  /* parse global settings */
  if (parse_global_settings(conf)) {
    return -1;
  }

  return 0;
}

int init_config(hsyncd_config_t conf, const char *infile)
{
  dbuffer_t content = NULL ;
  int err = 0;


  if (load_file(infile,&content) || !(conf->m_root=jsons_parse(content))) 
    err = -1;

  drop_dbuffer(content);
  
  if (err)
    return -1;

  if (parse_content(conf)) {
    log_error("parse config content fail\n");
    return -1;
  }

  return 0;
}

int free_config(hsyncd_config_t conf)
{
  jsons_release(conf->m_root);

  return 0;
}

