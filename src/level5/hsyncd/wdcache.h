#ifndef __WDCACHE_H__
#define __WDCACHE_H__

#include "rbtree.h"
#include "dbuffer.h"
#include "proto.h"
#include "objpool.h"


struct wdcache_s {
  int wd ;

  dbuffer_t pathname ;

  int fmt_id ;

  struct rb_node node ;

  struct list_head pool_item;

} __attribute__((__aligned__(64))) ;

typedef struct wdcache_s* wdcache_t ;


struct wdcache_entry_s {
  union {
    struct rb_root root;
  } u;

  size_t count ;

  objPool_t pool ;
} ;
typedef struct wdcache_entry_s* wdcache_entry_t ;


#define for_each_wdcaches(entry,pos,n)  \
  rbtree_postorder_for_each_entry_safe(pos,n,&(entry)->u.root,node) 

extern wdcache_t get_wdcache(wdcache_entry_t entry, const int wd);

extern const char* get_wdcache_path(wdcache_t p);

extern int get_wdcache_fmtid(wdcache_t p) ;

extern int add_wdcache(wdcache_entry_t entry, const int wd, const char *pathname, int fmt_id);

extern int drop_wdcache(wdcache_entry_t entry, const int wd);

extern int init_wdcache_entry(wdcache_entry_t entry);

extern int release_all_wdcaches(wdcache_entry_t entry);

extern size_t get_wdcache_count(wdcache_entry_t entry);

extern void iterate_wdcaches(wdcache_entry_t entry);

#endif /* __WDCACHE_H__*/
