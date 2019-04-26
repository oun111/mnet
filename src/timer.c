
#include <stdint.h>
#include <stdlib.h>
#include "kernel.h"
#include "timer.h"
#include "log.h"



int init_timer_entry(simple_timer_entry_t entry)
{
  INIT_LIST_HEAD(&entry->list);

  entry->num_timers = 0;

  return 0;
}

int register_simple_timer(simple_timer_entry_t entry, simple_timer_t tm)
{
  if (tm->timeouts==0) {
    log_error("timeout seconds invalid\n");
    return -1;
  }

  if (!tm->cb) {
    log_error("no timer function defined\n");
    return -1;
  }

  tm->sec_count = 0;

  list_add(&tm->upper,&entry->list);

  log_info("timer '%s' registered\n",tm->desc);

  return 0;
}

int scan_simple_timer_list(simple_timer_entry_t entry, void *net)
{
  simple_timer_t pos, n ;


  list_for_each_entry_safe(pos,n,&entry->list,upper) {

    if (pos->sec_count++ > pos->timeouts) {
      pos->cb(net,(void*)(uintptr_t)pos->timeouts);
      pos->sec_count = 0;
    }
  }

  return 0;
}

