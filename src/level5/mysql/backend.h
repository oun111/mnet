#ifndef __BACKEND_H__
#define __BACKEND_H__

#include "rbtree.h"
#include "dbuffer.h"
#include "mysqls.h"
#include "proto.h"
#include "task.h"


enum {
  st_empty,
  st_greeting_recv,
  st_try_login,
  //st_relogin,
  st_login_ok,
  st_login_fail,
} ;

struct backend_s {
  int fd ;

  int no;

  char addr[32];
  int port ;

  dbuffer_t schema;

  dbuffer_t usr ;
  dbuffer_t pwd ;

  int status ;  

  task_t task ;
  int is_use;

  int eof_count ;

  struct mysqlclient_proto_s prot ;

  /* connection item of this backend */
  connection_t conn ;

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


extern int create_backend(backend_entry_t entry, int fd, int no, 
    const char* host, int port, const char* sch, const char* usr, 
    const char *pwd) ;

extern int drop_backend(backend_entry_t entry, int fd);

extern int init_backend_entry(backend_entry_t entry);

extern int release_all_backends(backend_entry_t entry);

extern backend_t get_backend(backend_entry_t entry, int fd);

extern int set_backend_state(backend_t p, int st);

extern backend_t find_backend_by_num(backend_entry_t entry, int num, int *in_use);

extern void set_backend_unuse(backend_t p);

extern void set_backend_usage(backend_t p, task_t tsk);

extern void show_backend_in_use(backend_entry_t entry);

extern size_t get_backend_count(backend_entry_t entry) ;

extern void get_all_backend_num(backend_entry_t entry, int bk_num_list[], int bk_count);

#endif /* __BACKEND_H__*/
