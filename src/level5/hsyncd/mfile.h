#ifndef __MFILE_H__
#define __MFILE_H__

#include "rbtree.h"
#include "dbuffer.h"
#include "proto.h"
//#include "pay_data.h"
#include "objpool.h"


struct monitor_file_s {
  dbuffer_t filename ;

  size_t file_offset ;

  //size_t rp ;

  //size_t cont_size ;

  dbuffer_t cont_buf ;

  struct rb_node node ;

  struct list_head pool_item;

} __attribute__((__aligned__(64))) ;

typedef struct monitor_file_s* mfile_t ;


struct monitor_file_entry_s {
  union {
    struct rb_root root;
  } u;

  size_t count ;

  size_t sync_threshold ;

  objPool_t pool ;
} ;
typedef struct monitor_file_entry_s* mfile_entry_t ;


#define for_each_mfiles(entry,pos,n)  \
  rbtree_postorder_for_each_entry_safe(pos,n,&(entry)->u.root,node) 



extern mfile_t get_mfile(mfile_entry_t entry, const char *f);

extern int load_mfile(mfile_entry_t entry, const char *f, size_t size, mfile_t *pf);

extern int rename_mfile(mfile_entry_t entry, const char *oldfile, const char *newfile);

extern int drop_mfile(mfile_entry_t entry, const char *f);

extern int init_mfile_entry(mfile_entry_t entry, size_t st, ssize_t pool_size);

extern int release_all_mfiles(mfile_entry_t entry);

extern size_t get_mfile_count(mfile_entry_t entry);

extern bool is_mfile_sync(mfile_entry_t entry, mfile_t p);

extern void iterate_mfiles(mfile_entry_t entry);

#endif /* __MFILE_H__*/
