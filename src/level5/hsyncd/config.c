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
  const char *hbaseAddr;
  const char *hbasePort ;
  const char *workerCount ;
} 
g_confKW = {
  .gsSec       = "Globals",
  .monitorPath = "monitorPaths",
  .hbaseAddr   = "hbaseAddress",
  .hbasePort   = "hbasePort",
  .workerCount = "workers",
} ;



static 
int parse_global_settings(hsyncd_config_t conf)
{
  //size_t vl = 0L;
  //char *pv = NULL;
  jsonKV_t *pg = jsons_find(conf->m_root,g_confKW.gsSec), *p= 0;
  global_setting_t ps = &conf->m_globSettings ;


  if (!pg) {
    log_error("entry '%s' not found\n",g_confKW.gsSec);
    return -1;
  }

  bzero(ps,sizeof(*ps));

  // monitor settings
  p = jsons_find(pg,g_confKW.monitorPath);
  if (p) {
    if (conf->m_globSettings.monitor_paths)
      delete_tree_map(conf->m_globSettings.monitor_paths);

    conf->m_globSettings.monitor_paths = jsons_to_treemap(p) ;
  }

  // hbase client settings
  p = jsons_find(pg,g_confKW.hbaseAddr);
  if (p) {
    size_t vl = 0L;
    char *v = jsons_string(p->value,&vl);

    vl = vl<sizeof(conf->m_globSettings.hbaseAddr)?
      vl:sizeof(conf->m_globSettings.hbaseAddr);
    strncpy(conf->m_globSettings.hbaseAddr,v,vl);
    conf->m_globSettings.hbaseAddr[vl] = '\0';
  }

  p = jsons_find(pg,g_confKW.hbasePort);
  if (p) {
    conf->m_globSettings.hbasePort = atoi(p->value);
  }

  // workers count
  p = jsons_find(pg,g_confKW.workerCount);
  if (p) {
    conf->m_globSettings.workerCount = atoi(p->value);
  }

  return 0;
}

void get_hbase_client_settings(hsyncd_config_t conf, char *a, 
                              size_t la, int *p)
{
  size_t vl = la<sizeof(conf->m_globSettings.hbaseAddr)?
    la:sizeof(conf->m_globSettings.hbaseAddr);

  strncpy(a,conf->m_globSettings.hbaseAddr,vl);
  a[vl] = '\0';

  *p = conf->m_globSettings.hbasePort;
}

int get_worker_count(hsyncd_config_t conf)
{
  return conf->m_globSettings.workerCount ;
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

  conf->m_globSettings.monitor_paths = NULL ;

  if (parse_content(conf)) {
    log_error("parse config content fail\n");
    return -1;
  }

  return 0;
}

int free_config(hsyncd_config_t conf)
{
  if (conf->m_globSettings.monitor_paths)
    delete_tree_map(conf->m_globSettings.monitor_paths);

  jsons_release(conf->m_root);

  return 0;
}

