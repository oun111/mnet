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
  const char *mysql;
  const char *address;
  const char *port;
  const char *db;
  const char *usr;
  const char *pwd;
} 
g_confKW = {
  .gsSec        = "Globals",
  .mysql        = "Mysql",
  .address      = "address",
  .port         = "port",
  .db           = "db",
  .usr          = "usr",
  .pwd          = "pwd",
} ;


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

mysql_conf_t get_mysql_configs(alipayChk_config_t conf)
{
  return &conf->mysql_conf;
}

int process_mysql_configs(alipayChk_config_t conf)
{
  char *pstr = (char*)g_confKW.mysql ;
  jsonKV_t *pr = jsons_find(conf->m_root,pstr);
  mysql_conf_t pcfg = &conf->mysql_conf;


  if (!pr) {
    log_error("entry '%s' not found\n",pstr);
    return -1;
  }

  get_conf_str(pr,g_confKW.db,pcfg->db,sizeof(pcfg->db));

  get_conf_str(pr,g_confKW.usr,pcfg->usr,sizeof(pcfg->usr));

  get_conf_str(pr,g_confKW.pwd,pcfg->pwd,sizeof(pcfg->pwd));

  get_conf_str(pr,g_confKW.address,pcfg->address,sizeof(pcfg->address));

  get_conf_int(pr,g_confKW.port,&pcfg->port);

  return 0;
}

int init_config(alipayChk_config_t conf, const char *infile)
{
  dbuffer_t content = NULL ;
  int err = 0;


  if (load_file(infile,&content) || !(conf->m_root=jsons_parse(content))) 
    err = -1;

  drop_dbuffer(content);
  
  if (err)
    return -1;

  process_mysql_configs(conf);

  return 0;
}

int free_config(alipayChk_config_t conf)
{
  jsons_release(conf->m_root);

  return 0;
}

