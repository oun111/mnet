#include "log.h"
#include "instance.h"
#include "module.h"
#include "backend.h"
#include "order.h"
#include "merchant.h"
#include "global.h"


struct pay_global_s {

  struct backend_entry_s m_backends;

  pay_channels_entry_t m_paych ;

  struct order_entry_s m_orders ;

  struct merchant_entry_s m_merchant ;

} g_payGlobal ;


backend_entry_t get_backend_entry()
{
  return &g_payGlobal.m_backends;
}

pay_channels_entry_t get_pay_channels_entry()
{
  return g_payGlobal.m_paych ;
}

order_entry_t get_order_entry()
{
  return &g_payGlobal.m_orders ;
}

merchant_entry_t get_merchant_entry()
{
  return &g_payGlobal.m_merchant ;
}

static 
int pay_global_init(Network_t net)
{
  init_backend_entry(&g_payGlobal.m_backends,-1);

  init_pay_data(&g_payGlobal.m_paych);

  init_order_entry(&g_payGlobal.m_orders,-1);

  init_merchant_data(&g_payGlobal.m_merchant);

  log_info("done!\n");
  return 0;
}

static
void pay_global_release()
{
  release_all_orders(&g_payGlobal.m_orders);

  delete_pay_channels_entry(g_payGlobal.m_paych);

  release_all_backends(&g_payGlobal.m_backends);

  release_all_merchants(&g_payGlobal.m_merchant);
}


struct module_struct_s g_pay_global_mod = {

  .name = "pay global",

  .id = -1,

  .dyn_handle = NULL,

  .ssl = false,

  .opts[inbound_l5] = {
    .init    = pay_global_init,
    .release = pay_global_release,
  },
};
