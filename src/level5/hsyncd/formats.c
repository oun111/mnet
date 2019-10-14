#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include "formats.h"
#include "kernel.h"
#include "mm_porting.h"
#include "myrbtree.h"
#include "log.h"


static int compare(int id0, int id1)
{
  return id0>id1?1:id0<id1?-1:0 ;
}

formats_cb_t get_format(formats_entry_t entry, int fmt_id)
{
  formats_cb_t p = 0;

  if (!MY_RB_TREE_FIND(&entry->u.root,fmt_id,p,fmt_id,node,compare)) 
    return p ;

  return NULL ;
}

static
formats_cb_t create_empty_format(formats_entry_t entry, int fmt_id)
{
  formats_cb_t p = 0;

  if (!MY_RB_TREE_FIND(&entry->u.root,fmt_id,p,fmt_id,node,compare)) {
    log_debug("format with id '%d' exists\n",fmt_id);
    return p ;
  }

  //p = kmalloc(sizeof(struct formats_cb_s),0L);
  p = obj_pool_alloc(entry->pool,struct formats_cb_s);
  if (!p) {
    p = obj_pool_alloc_slow(entry->pool,struct formats_cb_s);
  }

  if (!p)
    return NULL ;

  p->fmt_id = fmt_id ;
  p->parser = NULL;
  p->init   = NULL;
  p->release= NULL;

  if (MY_RB_TREE_INSERT(&entry->u.root,p,fmt_id,node,compare)) {
    log_error("insert format by id %d fail\n",fmt_id);
    obj_pool_free(entry->pool,p);
    return NULL;
  }

  log_info("new format by id %d\n",fmt_id);

  entry->count++;

  return p;
}

int 
add_format(formats_entry_t entry, formats_cb_t fcb)
{
  formats_cb_t p = create_empty_format(entry,fcb->fmt_id);


  if (!p) {
    log_error("can't create format by id %d\n",fcb->fmt_id);
    return -1;
  }

  p->init   = fcb->init ;
  p->release= fcb->release ;
  p->parser = fcb->parser ;

  // invoke format init
  if (p->init)
    p->init();

  return 0;
}

static
int drop_format_internal(formats_entry_t entry, formats_cb_t p)
{
  if (p->release)
    p->release();

  rb_erase(&p->node,&entry->u.root);

  obj_pool_free(entry->pool,p);

  entry->count --;

  return 0;
}

int drop_format(formats_entry_t entry, int fmt_id)
{
  formats_cb_t p = get_format(entry,fmt_id);

  if (!p) {
    return -1;
  }

  drop_format_internal(entry,p);

  return 0;
}

static void register_formats(formats_entry_t entry)
{
  extern int register_fmt0(formats_entry_t entry);

  register_fmt0(entry);
}

int init_formats_entry(formats_entry_t entry)
{
  entry->u.root = RB_ROOT ;

  entry->count  = 0L;

  entry->pool = create_obj_pool("formats-pool",-1,struct formats_cb_s);

  register_formats(entry);

  log_debug("done!\n");

  return 0;
}

int release_all_formats(formats_entry_t entry)
{
  formats_cb_t pos,n;

  rbtree_postorder_for_each_entry_safe(pos,n,&entry->u.root,node) {
    drop_format_internal(entry,pos);
  }

  release_obj_pool(entry->pool,struct formats_cb_s);

  return 0;
}


size_t get_format_count(formats_entry_t entry)
{
  return entry->count ;
}

void iterate_formats(formats_entry_t entry)
{
  formats_cb_t pos,n;

  rbtree_postorder_for_each_entry_safe(pos,n,&entry->u.root,node) {
    log_debug("format with id %d\n",pos->fmt_id);
  }
}

common_format_t new_common_format()
{
  common_format_t pf = kmalloc(sizeof(struct common_format_s),0L);


  pf->row = alloc_default_dbuffer();

  INIT_LIST_HEAD(&pf->cf_list);

  INIT_LIST_HEAD(&pf->upper);

  return pf ;
}

cf_pair_t new_cf_pair()
{
  cf_pair_t pc = kmalloc(sizeof(struct cf_pair_s),0L);


  pc->cf = alloc_default_dbuffer();
  pc->val = alloc_default_dbuffer();

  return pc;
}

void save_cf_row_no(common_format_t pf, const char *rowno)
{
  write_dbuf_str(pf->row,rowno);
}

int insert_cf_pair(common_format_t pf, dbuffer_t cf, dbuffer_t val)
{
  cf_pair_t pc = kmalloc(sizeof(struct cf_pair_s),0L);


  pc->cf = alloc_default_dbuffer();
  write_dbuf_str(pc->cf,cf);

  pc->val = alloc_default_dbuffer();
  write_dbuf_str(pc->val,val);

  list_add_tail(&pc->upper,&pf->cf_list);
  return 0;
}

void insert_cf_pair2(common_format_t pf, cf_pair_t pc)
{
  list_add_tail(&pc->upper,&pf->cf_list);
}

int free_common_format(common_format_t pf)
{
  cf_pair_t pos, n ;


  list_for_each_entry_safe(pos,n,&pf->cf_list,upper) {
    drop_dbuffer(pos->cf);
    drop_dbuffer(pos->val);
    kfree(pos);
  }

  drop_dbuffer(pf->row);

  kfree(pf);

  return 0;
}

int read_format_data(formats_entry_t entry, int fmt_id, dbuffer_t inb, 
                     struct list_head *result_list)
{
  common_format_t fdata = 0;
  formats_cb_t pc = get_format(entry,fmt_id);

  if (!pc || !pc->parser) {
    log_error("no parser register for format %d\n",fmt_id);
    return -1;
  }


  while (pc->parser(inb,&fdata)==1) {
    if (fdata) 
      list_add(result_list,&fdata->upper);
  }

  return 0;
}

