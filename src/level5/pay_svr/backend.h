#ifndef __BACKEND_H__
#define __BACKEND_H__

#include "rbtree.h"
#include "dbuffer.h"
#include "proto.h"
#include "pay_data.h"
#include "objpool.h"


struct backend_s {
  int fd ;

  connection_t peer ;

  void *data ;

  struct rb_node node ;

  struct list_head pool_item;

} __attribute__((__aligned__(64))) ;

typedef struct backend_s* backend_t ;


struct backend_entry_s {
  union {
    struct rb_root root;
  } u;

  size_t num_backends ;

  objPool_t pool ;
} ;
typedef struct backend_entry_s* backend_entry_t ;


#define for_each_backends(entry,pos,n)  \
  rbtree_postorder_for_each_entry_safe(pos,n,&(entry)->u.root,node) 


extern int create_backend(backend_entry_t, int, connection_t, void*) ;

extern int drop_backend(backend_entry_t entry, int fd);

extern int init_backend_entry(backend_entry_t entry,ssize_t pool_size);

extern int release_all_backends(backend_entry_t entry);

extern backend_t get_backend(backend_entry_t entry, int fd);

extern size_t get_backend_count(backend_entry_t entry) ;


#endif /* __BACKEND_H__*/
