#ifndef __CONFIG_H__
#define __CONFIG_H__

#include "jsons.h"
#include "list.h"
#include "dbuffer.h"



struct paySvr_config_s {

  jsonKV_t *m_root ;

  tree_map_t chan_cfg ;

} __attribute__((__aligned__(64))) ;

typedef struct paySvr_config_s* paySvr_config_t ;


extern const char* get_bind_address(paySvr_config_t conf);

extern int get_listen_port(paySvr_config_t conf);

extern int get_notify_port(paySvr_config_t conf);

extern int init_config(paySvr_config_t conf, const char *infile);

extern int free_config(paySvr_config_t conf);

#endif /* __CONFIG_H__*/

