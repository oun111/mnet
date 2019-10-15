#ifndef __FORMATS_H__
#define __FORMATS_H__

#include "rbtree.h"
#include "dbuffer.h"
#include "proto.h"
#include "objpool.h"



struct cf_pair_s {
  dbuffer_t cf ;
  dbuffer_t val ;
  struct list_head upper ;
} ;
typedef struct cf_pair_s* cf_pair_t ;


struct common_format_s {
  dbuffer_t row ;

  dbuffer_t tbl ;

  struct list_head cf_list ;

  struct list_head upper ;
} ;
typedef struct common_format_s* common_format_t ;


typedef int (*fmt_parse)(dbuffer_t,common_format_t*);
typedef int (*fmt_init)();
typedef void(*fmt_release)();


struct formats_cb_s {
  int fmt_id ;

  fmt_init init ;
  fmt_release release ;
  fmt_parse parser;

  struct rb_node node ;

  struct list_head pool_item;

} __attribute__((__aligned__(64))) ;

typedef struct formats_cb_s* formats_cb_t ;


struct formats_entry_s {
  union {
    struct rb_root root;
  } u;

  size_t count ;

  objPool_t pool ;
} ;
typedef struct formats_entry_s* formats_entry_t ;


#define for_each_formats(entry,pos,n)  \
  rbtree_postorder_for_each_entry_safe(pos,n,&(entry)->u.root,node) 


extern int add_format(formats_entry_t entry, formats_cb_t fcb);

extern formats_cb_t get_format(formats_entry_t entry, int fmt_id);

extern int init_formats_entry(formats_entry_t entry);

extern int release_all_formats(formats_entry_t entry);

extern int read_format_data(formats_entry_t entry, int fmt_id, dbuffer_t inb, 
                            struct list_head *result_list);

extern common_format_t new_common_format();

extern int free_common_format(common_format_t pf);

extern cf_pair_t new_cf_pair();

extern void insert_cf_pair2(common_format_t pf, cf_pair_t pc);

extern void save_cf_row_no(common_format_t pf, const char *rowno);

extern void save_cf_table_name(common_format_t pf, const char *tbl);

#endif /* __FORMATS_H__*/
