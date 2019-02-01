#ifndef __BACKEND_H__
#define __BACKEND_H__

#include "rbtree.h"
#include "dbuffer.h"
#include "proto.h"
#include "pay_data.h"


struct backend_s {
  int fd ;

  connection_t peer ;

  pay_data_t pd ;

  struct rb_node node ;

} __attribute__((__aligned__(64))) ;

typedef struct backend_s* backend_t ;


struct backend_entry_s {
  union {
    struct rb_root root;
  } u;

  size_t num_backends ;
} ;
typedef struct backend_entry_s* backend_entry_t ;


#define for_each_backends(entry,pos,n)  \
  rbtree_postorder_for_each_entry_safe(pos,n,&(entry)->u.root,node) 


extern int create_backend(backend_entry_t, int, connection_t, pay_data_t) ;

extern int drop_backend(backend_entry_t entry, int fd);

extern int init_backend_entry(backend_entry_t entry);

extern int release_all_backends(backend_entry_t entry);

extern backend_t get_backend(backend_entry_t entry, int fd);

extern size_t get_backend_count(backend_entry_t entry) ;


#endif /* __BACKEND_H__*/
