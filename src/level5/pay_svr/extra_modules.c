#include "instance.h"
#include "module.h"
#include "kernel.h"


// TODO: extra modules here
extern struct module_struct_s g_alipay_ssl_outbound_mod ;


static
module_t g_module_list[] = 
{
  &g_alipay_ssl_outbound_mod,
};


void register_extra_modules()
{
  for (int i=0;i<ARRAY_SIZE(g_module_list);i++) {
    register_module(g_module_list[i]);
  }
}

