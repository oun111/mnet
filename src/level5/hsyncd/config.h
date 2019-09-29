#ifndef __CONFIG_H__
#define __CONFIG_H__

#include "jsons.h"


struct global_setting_s {
  char monitorPath[PATH_MAX];
} ;
typedef struct global_setting_s* global_setting_t ;

struct hsyncd_config_s {

  jsonKV_t *m_root ;

  struct global_setting_s m_globSettings;

} __attribute__((__aligned__(64))) ;

typedef struct hsyncd_config_s* hsyncd_config_t ;


extern const char* get_monitor_path(hsyncd_config_t conf);

extern int init_config(hsyncd_config_t conf, const char *infile);

extern int free_config(hsyncd_config_t conf);

#endif /* __CONFIG_H__*/

