#ifndef __CONFIG_H__
#define __CONFIG_H__

#include "jsons.h"


struct global_setting_s {
  char gnuClassPath[500];
  char prjPath[500];
  char prjClass[64];
  int  bindAddrInJni;
  char bindAddr[32];
  int listenPort;
} ;
typedef struct global_setting_s* global_setting_t ;

struct myJvm_config_s {

  jsonKV_t *m_root ;

  struct global_setting_s m_globSettings;

} __attribute__((__aligned__(64))) ;

typedef struct myJvm_config_s* myJvm_config_t ;


extern const char* get_gnu_classpath(myJvm_config_t conf);

extern const char* get_prj_path(myJvm_config_t conf);

extern const char* get_prj_class(myJvm_config_t conf);

extern const char* get_bind_address(myJvm_config_t conf);

extern int get_listen_port(myJvm_config_t conf);

extern int init_config(myJvm_config_t conf, const char *infile);

extern int free_config(myJvm_config_t conf);

#endif /* __CONFIG_H__*/

