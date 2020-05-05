#include <sys/socket.h>
#include "socket.h"
#include "connection.h"
#include "notify.h"
#include "instance.h"
#include "module.h"
#include "timer.h"
#include "log.h"



static int notify_rx(Network_t net,connection_t pconn)
{
  int ret = -1;
  unsigned char notes[32];


  if (!pconn) {
    return -1;
  }

  ret = recv(pconn->fd,notes,sizeof(notes),0);
  if (ret<=0) {
    return -1;
  }

  for (int i=0;i<ret;i++) {
    switch (notes[i]) {
      case nt_bh_timer: bh_timers_proc(CURRENT_TIMERS);
                        break ;
      case nt_sig_stop: gracefully_exit();
                        break ;
      default: break;
    }
  }

  return 0;
}

static void notify_close(Network_t net,connection_t pconn)
{
}

static
struct module_struct_s g_module = {

  .name = "notifies",

  .id = -1,

  .dyn_handle = NULL,

  .ssl = false,

  .opts[local_l4] = {
    .rx = notify_rx,
    .close = notify_close,
  },
} ;

int init_notify_entry(notify_entry_t entry, Network_t net)
{
  if (new_socketpair(net->m_efd,entry->fds)) {
    return -1;
  }

  entry->rxfd = &entry->fds[0];
  entry->txfd = &entry->fds[1];

  register_module(THIS_MODULE);

  net->reg_local(net,*entry->rxfd,THIS_MODULE->id);

  return 0;
}

void release_notify_entry(notify_entry_t entry)
{
  close(entry->fds[0]);
  close(entry->fds[1]);
}

int notify_tx_fd(notify_entry_t entry)
{
  return entry->txfd?*entry->txfd:-1 ;
}

int notify_rx_fd(notify_entry_t entry)
{
  return entry->rxfd?*entry->rxfd:-1 ;
}

int send_notify(unsigned char *notes, size_t sz)
{
  int fd = notify_tx_fd(CURRENT_NOTIFY);

  return send(fd,notes,sz,0);
}

