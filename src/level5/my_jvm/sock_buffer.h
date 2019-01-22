#ifndef __SOCK_BUFFER_H__
#define __SOCK_BUFFER_H__

#include "rbtree.h"
#include "dbuffer.h"
#include "list.h"


struct sock_buffer_s {
  int fd ;

  int valid ;

  dbuffer_t in ;
  dbuffer_t out ;

  void *net ;
  void *conn ;

  struct rb_node node ;

  struct list_head qitem ;

  pthread_mutex_t mtx ;
  pthread_cond_t  cond;
} ;
typedef struct sock_buffer_s* sbuf_t ;

struct sock_buffer_entry_s {
  union {
    struct rb_root root;
  } u;

  int sbuf_count ;

  struct list_head qhead ;
} ;
typedef struct sock_buffer_entry_s* sbuf_entry_t ;



extern int init_sock_buffer_entry(sbuf_entry_t entry);

extern sbuf_t create_sock_buffer(sbuf_entry_t entry, int fd, void *net, void *conn);

extern int drop_sock_buffer(sbuf_entry_t entry, int fd);

extern int release_all_sock_buffers(sbuf_entry_t entry);

extern sbuf_t get_sock_buffer(sbuf_entry_t entry, int fd);

extern int get_sock_buffers_count(sbuf_entry_t entry);

extern void set_sock_buffer_valid(sbuf_t psb, int v);

extern int is_sock_buffer_valid(sbuf_t psb);

extern int sbufq_pop(sbuf_entry_t entry);

extern void sbufq_push(sbuf_entry_t entry, sbuf_t p);

#endif /* __SOCK_BUFFER_H__*/
