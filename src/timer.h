#ifndef __TIMER_H__
#define __TIMER_H__


#include "list.h"


typedef int(*simple_timer_func)(void*,void*);

struct simple_timer_s {

  char *desc;

  simple_timer_func cb ;

  unsigned int timeouts ;

  unsigned int sec_count ;

  bool bottom_half;

  struct list_head upper ;
  struct list_head bh_upper;

} ;
typedef struct simple_timer_s* simple_timer_t ;


struct simple_timer_entry_s {

  struct list_head list ;

  // bottom half list
  struct list_head bh_list ;

  int num_timers;

  pthread_t idle_t ;

  // send bottom-half notify to this fd
  int note_fd ; 

  void *net ;

  int stop ;
} ;
typedef struct simple_timer_entry_s* simple_timer_entry_t ;


extern int init_timer_entry(void *net, simple_timer_entry_t entry);

extern void release_timer_entry(simple_timer_entry_t entry);

extern int register_simple_timer(simple_timer_entry_t entry, simple_timer_t tm);

extern int bh_timers_proc(simple_timer_entry_t entry);

#endif /* __TIMER_H__*/
