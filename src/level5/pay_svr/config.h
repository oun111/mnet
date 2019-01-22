#ifndef __CONFIG_H__
#define __CONFIG_H__

#include "jsons.h"
#include "list.h"
#include "dbuffer.h"


struct channel_config_s {
  char type[64];
  dbuffer_t url;
  char merchant[128];
  dbuffer_t apikey;
  dbuffer_t pubkey;
  dbuffer_t privkey;
  struct list_head upper ;
} ;
typedef struct channel_config_s* channel_config_t ;

struct global_setting_s {
  char bindAddr[32];
  int listenPort;
  int notifyPort;
} ;
typedef struct global_setting_s* global_setting_t ;

struct paySvr_config_s {

  jsonKV_t *m_root ;

  struct global_setting_s m_globSettings;

  struct list_head m_channels ;

} __attribute__((__aligned__(64))) ;

typedef struct paySvr_config_s* paySvr_config_t ;


extern channel_config_t get_channel_conf(paySvr_config_t conf,const char *type);

extern const char* get_bind_address(paySvr_config_t conf);

extern int get_listen_port(paySvr_config_t conf);

extern int get_notify_port(paySvr_config_t conf);

extern int init_config(paySvr_config_t conf, const char *infile);

extern int free_config(paySvr_config_t conf);

#endif /* __CONFIG_H__*/

