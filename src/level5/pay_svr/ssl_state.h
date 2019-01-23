#ifndef __SSL_STATE_H__
#define __SSL_STATE_H__

#include "rbtree.h"
#include "dbuffer.h"
#include "proto.h"
#include "ssl.h"



enum sslst {
  s_none,
  s_connecting,
  s_normal
};

struct sslstate_s {
  int fd ;

  /* connection item of this sslstate */
  connection_t out_conn;

  unsigned char st;

  struct rb_node node ;

} __attribute__((__aligned__(64))) ;

typedef struct sslstate_s* sslstate_t ;


struct sslstate_entry_s {
  union {
    struct rb_root root;
  } u;

  size_t num_sslstates ;
} ;
typedef struct sslstate_entry_s* sslstate_entry_t ;


#define for_each_sslstates(entry,pos,n)  \
  rbtree_postorder_for_each_entry_safe(pos,n,&(entry)->u.root,node) 


extern sslstate_t get_sslstate(sslstate_entry_t entry, int fd);

extern int create_sslstate(sslstate_entry_t entry, int fd, connection_t pconn);

extern int drop_sslstate(sslstate_entry_t entry, int fd);

extern int init_sslstate_entry(sslstate_entry_t entry);

extern int release_all_sslstates(sslstate_entry_t entry);

extern size_t get_sslstate_count(sslstate_entry_t entry);

extern void update_sslstate_st(sslstate_entry_t entry, int fd, unsigned char st);

#endif /* __SSL_STATE_H__*/
