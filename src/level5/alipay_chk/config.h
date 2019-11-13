#ifndef __CONFIG_H__
#define __CONFIG_H__

#include "jsons.h"
#include "list.h"
#include "dbuffer.h"



struct mysql_config_s {
  char address[32];
  int port;
  char db[32];
  char usr[64];
  char pwd[256];
} ;
typedef struct mysql_config_s* mysql_conf_t ;


struct alipayChk_config_s {

  jsonKV_t *m_root ;

  struct mysql_config_s mysql_conf;

} __attribute__((__aligned__(64))) ;

typedef struct alipayChk_config_s* alipayChk_config_t ;


extern mysql_conf_t get_mysql_configs(alipayChk_config_t conf);

extern int init_config(alipayChk_config_t conf, const char *infile);

extern int free_config(alipayChk_config_t conf);

#endif /* __CONFIG_H__*/

