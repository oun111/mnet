#include "log.h"
#include "instance.h"
#include "module.h"
#include "backend.h"
#include "chan.h"


struct channels_global_data_t {

  struct backend_entry_s m_backends;

  pay_channels_entry_t m_paych ;

} g_chanGlobData ;


backend_entry_t get_backend_entry()
{
  return &g_chanGlobData.m_backends;
}

pay_channels_entry_t get_pay_channels_entry()
{
  return g_chanGlobData.m_paych ;
}


static 
int chan_global_init(Network_t net)
{
  init_backend_entry(&g_chanGlobData.m_backends,-1);

  init_pay_data(&g_chanGlobData.m_paych);

  log_info("done!\n");
  return 0;
}

static
void chan_global_release()
{
  release_all_backends(&g_chanGlobData.m_backends);

  delete_pay_channels_entry(g_chanGlobData.m_paych);
}


struct module_struct_s g_chan_global_mod = {

  .name = "pay channels global",

  .id = -1,

  .dyn_handle = NULL,

  .ssl = false,

  .opts[inbound_l5] = {
    .init    = chan_global_init,
    .release = chan_global_release,
  },
};
