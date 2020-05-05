
#include <stdint.h>
#include <stdlib.h>
#include "kernel.h"
#include "timer.h"
#include "log.h"
#include "notify.h"
#include "instance.h"


static
int scan_simple_timer_list(simple_timer_entry_t entry)
{
  simple_timer_t pos, n ;
  unsigned char bh = 0 ;


  list_for_each_entry_safe(pos,n,&entry->list,upper) {

    if (++pos->sec_count >= pos->timeouts) {

      // don't do bottom-halfs here
      if (pos->bottom_half) {
        bh = nt_bh_timer;
        continue ;
      }

      pos->cb(entry->net,(void*)(uintptr_t)pos->timeouts);
      pos->sec_count = 0;
    }
  }

  if (bh==nt_bh_timer) {
    send_notify(&bh,sizeof(bh));
  }

  return 0;
}

int bh_timers_proc(simple_timer_entry_t entry)
{
  simple_timer_t pos, n ;

  list_for_each_entry_safe(pos,n,&entry->bh_list,bh_upper) {

    if (++pos->sec_count >= pos->timeouts) {
      pos->cb(entry->net,(void*)(uintptr_t)pos->timeouts);
      pos->sec_count = 0;
    }
  }

  return 0;
}

static
void* idle_task(void *arg)
{
  simple_timer_entry_t entry = arg ;


  pthread_detach(pthread_self());

  while(!(entry->stop&1)) {

    scan_simple_timer_list(entry);
    sleep(1);
  }

  return 0;
}

int init_timer_entry(void *net, simple_timer_entry_t entry)
{
  INIT_LIST_HEAD(&entry->list);

  INIT_LIST_HEAD(&entry->bh_list);

  entry->num_timers = 0;

  entry->stop = 0;

  entry->net  = net ;

  pthread_create(&entry->idle_t,NULL,idle_task,entry);

  return 0;
}

void release_timer_entry(simple_timer_entry_t entry)
{
  entry->stop |= 0x1;
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

  if (tm->bottom_half==true)
    list_add(&tm->bh_upper,&entry->bh_list);

  log_info("timer '%s' registered\n",tm->desc);

  return 0;
}

