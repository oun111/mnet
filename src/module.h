#ifndef __MODULE_H__
#define __MODULE_H__

#include "proto.h"

typedef void (*init_call)(int,char*[]);
typedef void (*exit_call)();


extern init_call _init_start;
extern init_call _init_end;

extern exit_call _exit_start;
extern exit_call _exit_end;


struct module_struct_s {
  char name[64];

  proto_opt opts[max_protos];

  int id ;

  // it's a dynamic module
  void *dyn_handle;

  void *extra ;

  bool ssl ;
} ;

typedef struct module_struct_s* module_t ;


#define MODULE_INIT(fn)  \
  init_call __f_ ## fn __attribute__((unused, section(".myinit"))) = fn;

#define MODULE_EXIT(fn)  \
  exit_call __f_ ## fn __attribute__((unused, section(".myexit"))) = fn;


#define THIS_MODULE   (&g_module)


extern int init_module_list(int argc, char *argv[]);

extern void release_module_list();

extern int register_module(module_t src_mod);

extern module_t get_module(int mod_id);

extern int get_module_id(const char *mod_name);

#endif /* __MODULE_H__*/

