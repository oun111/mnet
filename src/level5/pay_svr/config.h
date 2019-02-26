#ifndef __CONFIG_H__
#define __CONFIG_H__

#include "jsons.h"
#include "list.h"
#include "dbuffer.h"



struct paySvr_config_s {

  jsonKV_t *m_root ;

  tree_map_t chan_root,chan_cfg ;

  tree_map_t mch_root,merchant_cfg ;

} __attribute__((__aligned__(64))) ;

typedef struct paySvr_config_s* paySvr_config_t ;


extern int get_bind_address(paySvr_config_t conf, char *host);

extern int get_listen_port(paySvr_config_t conf);

extern int get_notify_port(paySvr_config_t conf);

extern int get_myredis_info(paySvr_config_t conf, char *host, int *port, 
                            char *dataTbl, char *cfgTbl);

extern int process_merchant_configs(paySvr_config_t conf, dbuffer_t);

extern int process_channel_configs(paySvr_config_t conf, dbuffer_t);

extern int init_config(paySvr_config_t conf, const char *infile);

extern int free_config(paySvr_config_t conf);

#endif /* __CONFIG_H__*/

