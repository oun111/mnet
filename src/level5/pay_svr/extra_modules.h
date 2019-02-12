#ifndef __EXTRA_MODULES_H__
#define __EXTRA_MODULES_H__



// TODO: extra modules here
extern struct module_struct_s g_alipay_mod ;

extern struct module_struct_s g_chan_global_mod;



static
module_t g_module_list[] = 
{
  &g_chan_global_mod,

  &g_alipay_mod,
};


static void register_extra_modules()
{
  for (int i=0;i<ARRAY_SIZE(g_module_list);i++) {
    register_module(g_module_list[i]);
  }
}


#endif /* __EXTRA_MODULES_H__*/
