#ifndef __TIMER_H__
#define __TIMER_H__


#include "list.h"


typedef int(*simple_timer_func)(void*,void*);

struct simple_timer_s {

  char *desc;

  simple_timer_func cb ;

  unsigned int timeouts ;

  struct list_head upper ;

} ;
typedef struct simple_timer_s* simple_timer_t ;


struct simple_timer_entry_s {

  struct list_head list ;

  int num_timers;
} ;
typedef struct simple_timer_entry_s* simple_timer_entry_t ;


extern int init_timer_entry(simple_timer_entry_t entry);

extern int register_simple_timer(simple_timer_entry_t entry, simple_timer_t tm);

extern int scan_simple_timer_list(simple_timer_entry_t entry, void *net);


#endif /* __TIMER_H__*/
