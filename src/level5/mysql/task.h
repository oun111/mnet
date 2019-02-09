#ifndef __TASK_H__
#define __TASK_H__

#include <unistd.h>
#include "list.h"
#include "dbuffer.h"
#include "objpool.h"
#include "connection.h"

struct __attribute__((__aligned__(64))) task_s {

  int is_use;

  dbuffer_t backend_numbers ;

  size_t backend_count;

  int ref_count;

  //int total_eof ;
  int ptn_pos ;

  unsigned char sn ; 

  connection_t src ;

  dbuffer_t req ;

  struct list_head taskq_item ;

  struct list_head pool_item;

} ;

typedef struct task_s* task_t ;


struct task_entry_s {
  objPool_t pool;

  struct list_head taskq ;
} ;

typedef struct task_entry_s* task_entry_t ;



extern int init_task_pool(task_entry_t entry, ssize_t pool_size);

extern void release_task_pool(task_entry_t entry);

extern task_t alloc_task(task_entry_t entry, int backend_count);

extern int free_task(task_entry_t entry, task_t ptask);

extern void save_task_bk_num(task_t ptask, int pos, int no);

extern int get_task_bk_num(task_t ptask, int pos);

extern void task_ref_set(task_t ptask, int val);

extern void task_ref_add(task_t ptask);

extern void task_ref_sub(task_t ptask);

extern int get_task_ref(task_t ptask);

extern void save_task_req(task_t ptask, char *inb, size_t sz_in);

extern int get_task_req(task_t ptask, char **inb, size_t *sz_in);

extern void task_enqueue(task_entry_t entry, task_t tsk);

extern task_t task_dequeue(task_entry_t entry);

extern void show_pool_item_count(task_entry_t entry);

#endif /* __TASK_H__*/
