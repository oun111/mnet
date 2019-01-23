#include <string.h>
#include <dirent.h>
#include <dlfcn.h>
#include "L4.h"
#include "instance.h"
#include "log.h"


#define init_static_modules(argc,argv)  do{\
  init_call *init_ptr = &_init_start;  \
  for (; init_ptr < &_init_end; init_ptr++) \
    (*init_ptr)(argc,argv);  \
} while(0)

#define static_modules_exit()  do{\
  exit_call *exit_ptr = &_exit_start;  \
  for (; exit_ptr < &_exit_end; exit_ptr++) \
    (*exit_ptr)();  \
} while(0)

#define get_static_modules_count()  ((size_t)((&_init_end - &_init_start)))



struct module_list_t {

  module_t list ;

  int id_counter ;

  size_t total_modules ;

  char lib_path[PATH_MAX];
} 
g_module_list = {

  .list = NULL,

  .id_counter = 0,

  .total_modules = 0L,

  .lib_path = "./",
} ;



static
int parse_cmd_line(int argc, char *argv[])
{
  for (int i=1; i<argc; i++) {
    if (!strcmp(argv[i],"-l") && (i+1)<argc) {
      strcpy(g_module_list.lib_path,argv[i+1]);
    }
    else if (!strcmp(argv[i],"-h")) {
      printf("module list help message: \n");
      printf("-l: dynamic modules path\n");
    }
  }

  return 0;
}

static
int get_dynamic_modules_count()
{
  char buf[PATH_MAX+20] = "";
  FILE *ps = NULL ;


  snprintf(buf,PATH_MAX+20,"ls %s/*%s | wc -l", 
      g_module_list.lib_path, DY_MOD_EXT);

  ps = popen(buf,"r");

  fgets(buf,PATH_MAX,ps);
  pclose(ps);
  return atoi(buf);
}

static
int init_dynamic_modules(int argc, char *argv[])
{
  DIR *dp = opendir(g_module_list.lib_path) ;
  struct dirent *dir = 0;
  void *handle = 0;


  if (!dp) {
    char *p = g_module_list.lib_path;
    log_error("cant open dynamic module dir '%s'\n",p);
    return -1;
  }

  while ((dir=readdir(dp))) {

    char path[PATH_MAX] = "", init_fn_name[128] = "",mod_name[64], *pos = 0;


    if (dir->d_type==DT_DIR)
      continue ;

    pos = strstr(dir->d_name, DY_MOD_EXT);
    if (!pos || pos==dir->d_name)
      continue ;

    strcpy(path,g_module_list.lib_path);
    strcat(path,"/");
    strcat(path,dir->d_name);

    handle = dlopen(path, RTLD_LAZY /*RTLD_NOW*/);
    if (!handle) {
      pos = path ;
      log_error("cant dynamic load module '%s': %s\n",pos,dlerror());
      continue ;
    }

    strncpy(mod_name,dir->d_name,strlen(dir->d_name)-strlen(DY_MOD_EXT));
    mod_name[strlen(dir->d_name)-strlen(DY_MOD_EXT)] = '\0';
    snprintf(init_fn_name,128,"%s_module_init",mod_name);

    init_call pf = (init_call)dlsym(handle,init_fn_name);

    if (!pf) {
      log_error("cant locate module init function '%s'\n",init_fn_name);
      continue ;
    }

    pf(argc,argv);

    // fill back the module infos
    module_t current_module = &g_module_list.list[g_module_list.id_counter-1];

    current_module->dyn_handle = handle ;
    strcpy(current_module->name,mod_name);
  }

  closedir(dp);

  return 0;
}

int release_dynamic_modules()
{
  for (int i=0;i<g_module_list.total_modules; i++) {

    void *handle = g_module_list.list[i].dyn_handle;
    char exit_fn_name[128] = "";


    if (!handle)
      continue ;

    snprintf(exit_fn_name,128,"%s_module_exit",g_module_list.list[i].name);
    exit_call pf = (exit_call)dlsym(handle,exit_fn_name);

    if (!pf) {
      log_error("cant locate module exit function '%s'\n",exit_fn_name);
      continue ;
    }

    pf();

    dlclose(handle);
  }

  return 0;
}

int init_module_list(int argc, char *argv[])
{
  size_t nDyn = 0L, nStatic = 0L;


  parse_cmd_line(argc,argv);

  nDyn = get_dynamic_modules_count()*2 ;
  nStatic = get_static_modules_count() ;


  g_module_list.total_modules = nDyn*2 + nStatic ;

  log_debug("total module list size: %zu, static %zu, dynamic %zu\n",
            g_module_list.total_modules,nStatic,nDyn);

  g_module_list.list = kmalloc(sizeof(struct module_struct_s)
                               *g_module_list.total_modules,0L);


  init_static_modules(argc,argv);

  init_dynamic_modules(argc,argv);

  return 0;
}

void release_module_list()
{
  static_modules_exit();

  release_dynamic_modules();

  if (g_module_list.list)
    kfree(g_module_list.list);
}

int register_module(module_t src_mod)
{
  const size_t nModules = g_module_list.total_modules;
  module_t pmod = 0/*, src_mod = (module_t)mod*/;
  int *pcnt = 0;


  if (g_module_list.id_counter>=nModules) {
    log_error("much too modules\n");
    return -1;
  }

  pcnt = &g_module_list.id_counter ;
  pmod = &g_module_list.list[*pcnt];
  src_mod->id = (*pcnt)++ ;
  memcpy(pmod,src_mod,sizeof(struct module_struct_s));

  if (!pmod->opts[local_l4].rx)
    pmod->opts[local_l4].rx = tcp_accept ;

  if (!pmod->opts[local_l4].close)
    pmod->opts[local_l4].close = tcp_close ;

  if (!pmod->opts[normal_l4].rx)
    pmod->opts[normal_l4].rx = tcp_rx ;

  if (!pmod->opts[normal_l4].tx)
    pmod->opts[normal_l4].tx = tcp_tx ;

  if (!pmod->opts[normal_l4].close)
    pmod->opts[normal_l4].close = tcp_close ;

  log_info("module '%s' id %d init done! \n",pmod->name,pmod->id);

  return 0;
}

module_t get_module(int mod_id)
{
  //log_debug("module id: %d\n",mod_id);
  return mod_id>=0 && mod_id<g_module_list.total_modules ? 
         &g_module_list.list[mod_id]:NULL ;
}

int get_module_id(const char *mod_name)
{
  for (int i=0;i<g_module_list.total_modules;i++)
    if (!strcmp(g_module_list.list[i].name,mod_name)) {
      //log_debug("module name '%s' -> id '%d'\n",mod_name,g_module_list.list[i].id);
      return g_module_list.list[i].id ;
    }

  return -1;
}

void* get_module_extra(int mod_id)
{
  if (mod_id>=0 && mod_id<g_module_list.total_modules)
    return g_module_list.list[mod_id].extra ;

  return NULL ;
}

