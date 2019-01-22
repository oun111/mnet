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
  const char *gnuClassPath;
  const char *prjPath ;
  const char *prjClass;
  const char *bindAddr;
  const char *listenPort;
} 
g_confKW = {
  .gsSec        = "Globals",
  .gnuClassPath = "GnuClassPath",
  .prjPath      = "ProjectPath",
  .prjClass     = "ProjectClass",
  .bindAddr     = "BindAddress",
  .listenPort   = "ListenPort",
} ;



static 
int parse_global_settings(myJvm_config_t conf)
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

#if 0
  list_for_each_entry(p,&pg->children,upper) {

    if (!strcmp(p->key,g_confKW.gnuClassPath)) {
      strncpy(ps->gnuClassPath,p->value,sizeof(ps->gnuClassPath));
    }
    else if (!strcmp(p->key,g_confKW.prjPath)) {
      strncpy(ps->prjPath,p->value,sizeof(ps->prjPath));
    }
    else if (!strcmp(p->key,g_confKW.prjClass)) {
      strncpy(ps->prjClass,p->value,sizeof(ps->prjClass));
    }
    else if (!strcmp(p->key,g_confKW.bindAddr)) {
      strncpy(ps->bindAddr,p->value,sizeof(ps->bindAddr));
    }
    else if (!strcmp(p->key,g_confKW.listenPort)) {
      ps->listenPort = atoi(p->value);
    }
  }
#else
  p = jsons_find(pg,g_confKW.gnuClassPath);
  if (p) {
    pv = jsons_string(p->value,&vl);
    strncpy(ps->gnuClassPath,pv,vl);
  }

  p = jsons_find(pg,g_confKW.prjPath);
  if (p) {
    pv = jsons_string(p->value,&vl);
    strncpy(ps->prjPath,pv,vl);
  }

  p = jsons_find(pg,g_confKW.prjClass);
  if (p) {
    pv = jsons_string(p->value,&vl);
    strncpy(ps->prjClass,pv,vl);
  }

  p = jsons_find(pg,g_confKW.bindAddr);
  if (p) {
    pv = jsons_string(p->value,&vl);
    strncpy(ps->bindAddr,pv,vl);
  }

  p = jsons_find(pg,g_confKW.listenPort);
  if (p) {
    ps->listenPort = jsons_integer(p->value);
  }
#endif

  return 0;
}

const char* get_gnu_classpath(myJvm_config_t conf)
{
  return conf->m_globSettings.gnuClassPath ;
}

const char* get_prj_path(myJvm_config_t conf)
{
  return conf->m_globSettings.prjPath ;
}

const char* get_prj_class(myJvm_config_t conf)
{
  return conf->m_globSettings.prjClass ;
}

const char* get_bind_address(myJvm_config_t conf)
{
  return conf->m_globSettings.bindAddr ;
}

int get_listen_port(myJvm_config_t conf)
{
  return conf->m_globSettings.listenPort ;
}

static
int parse_content(myJvm_config_t conf)
{
  /* parse global settings */
  if (parse_global_settings(conf)) {
    return -1;
  }

  return 0;
}

int init_config(myJvm_config_t conf, const char *infile)
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

int free_config(myJvm_config_t conf)
{
  jsons_release(conf->m_root);

  return 0;
}

