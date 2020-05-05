#ifndef __NOTIFY_H__
#define __NOTIFY_H__

#include "proto.h"

enum note_types {
  nt_none,
  nt_bh_timer,  // notify from bottom-half timers
  nt_sig_stop,  // the stop/terminate signal
} ;

struct notify_entry_s {
  int fds[2] ;
  int *rxfd ;
  int *txfd ;
} ;
typedef struct notify_entry_s* notify_entry_t ;


extern int init_notify_entry(notify_entry_t entry, Network_t net);

extern int notify_tx_fd(notify_entry_t entry);

extern int notify_rx_fd(notify_entry_t entry);

extern int send_notify(unsigned char *notes, size_t sz);

#endif /* __NOTIFY_H__*/
