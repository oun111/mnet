#ifndef __INSTANCE_H__
#define __INSTANCE_H__

#include "proto.h"
#include "connection.h"
#include "notify.h"
#include "timer.h"


struct main_instance_s {

  char name[32];

  int is_master;

  size_t num_workers;

  pid_t *worker_pids;

  struct Network_s g_nets ;

  unsigned int worker_stop:1 ;

  int use_log:1 ;

  struct simple_timer_entry_s timers ;

  struct notify_entry_s notify;
} ;

extern struct main_instance_s g_inst ;

#define CURRENT_TIMERS  (&g_inst.timers)

#define CURRENT_NOTIFY  (&g_inst.notify)

extern int instance_start(int argc, char *argv[]);

extern int instance_stop();

extern Network_t get_current_net();

extern void set_proc_name(int argc, char *argv[], const char *newname);

extern void gracefully_exit();

#endif /* __INSTANCE_H__*/
