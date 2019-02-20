#include <string.h>
#include "instance.h"
#include "action.h"
#include "config.h"
#include "http_svr.h"
#include "module.h"




static
struct pay_svr_conf {

  char conf_path[PATH_MAX];

  struct paySvr_config_s m_conf;

} g_paySvrConf ;



paySvr_config_t get_running_configs()
{
  return &g_paySvrConf.m_conf;
}

static
int parse_cmd_line(int argc, char *argv[])
{
  //char *ptr = 0;

  for (int i=1; i<argc; i++) {
    if (!strcmp(argv[i],"-cp") && i+1<argc) {
      strcpy(g_paySvrConf.conf_path,argv[i+1]);
    }
    else if (!strcmp(argv[i],"-h")) {
      printf("help message\n");
      return 1;
    }
  }
  return 0;
}

static void register_extra_modules()
{
  extern struct module_struct_s g_alipay_mod ;
  extern struct module_struct_s g_pay_global_mod;

  register_module(&g_pay_global_mod);
  register_module(&g_alipay_mod);
}

void pay_svr_module_init(int argc, char *argv[])
{
  const char *host = NULL ;
  int port = 0, notify_port = 0;


  if (parse_cmd_line(argc,argv))
    return ;

  if (init_config(&g_paySvrConf.m_conf,g_paySvrConf.conf_path))
    return ;

  host        = get_bind_address(&g_paySvrConf.m_conf);
  notify_port = get_notify_port(&g_paySvrConf.m_conf);
  port        = get_listen_port(&g_paySvrConf.m_conf);

  // the http base entry
  __http_svr_entry(host,port,notify_port);

  register_extra_modules();
}

void pay_svr_module_exit()
{
  __http_svr_exit();

  free_config(&g_paySvrConf.m_conf);
}

