#ifndef __CONFIG_H__
#define __CONFIG_H__

#include "jsons.h"
#include "list.h"
#include "dbuffer.h"



struct paySvr_config_s {

  jsonKV_t *m_root ;

  tree_map_t chan_conf ;

  tree_map_t mch_conf ;

} __attribute__((__aligned__(64))) ;

typedef struct paySvr_config_s* paySvr_config_t ;


struct myredis_config_s {
  char host[32];
  int port ;
  char data_table[32]; // data table in redis
  char conf_table[32]; // config table in redis
} ;
typedef struct myredis_config_s* myredis_conf_t ;


struct mysql_config_s {
  char chan_config_table[32];
  char mch_config_table[32];
} ;
typedef struct mysql_config_s* mysql_conf_t ;


extern int get_bind_address(paySvr_config_t conf, char *host);

extern int get_listen_port(paySvr_config_t conf);

extern int get_notify_port(paySvr_config_t conf);

extern int get_myredis_configs(paySvr_config_t conf, myredis_conf_t pcfg);

extern int get_mysql_configs(paySvr_config_t conf, mysql_conf_t pcfg);

extern int process_merchant_configs(paySvr_config_t conf, dbuffer_t);

extern int process_channel_configs(paySvr_config_t conf, dbuffer_t);

extern int init_config(paySvr_config_t conf, const char *infile);

extern int free_config(paySvr_config_t conf);

#endif /* __CONFIG_H__*/

