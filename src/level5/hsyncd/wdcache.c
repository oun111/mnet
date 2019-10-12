#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include "wdcache.h"
#include "kernel.h"
#include "mm_porting.h"
#include "myrbtree.h"
#include "log.h"


static int compare(int w0, int w1)
{
  return w0>w1?1:w0<w1?-1:0;
}

wdcache_t get_wdcache(wdcache_entry_t entry, const int wd)
{
  wdcache_t p = 0;

  if (!MY_RB_TREE_FIND(&entry->u.root,wd,p,wd,node,compare)) 
    return p ;

  return NULL ;
}

int get_wdcache_fmtid(wdcache_t p)
{
  return p?p->fmt_id:-1;
}

const char* get_wdcache_path(wdcache_t p)
{
  return p?p->pathname:NULL;
}

static
wdcache_t create_empty_wdcache(wdcache_entry_t entry, const int wd)
{
  wdcache_t p = 0;

  if (!MY_RB_TREE_FIND(&entry->u.root,wd,p,wd,node,compare)) {
    log_debug("wd cache %d exists\n",wd);
    return p ;
  }

  //p = kmalloc(sizeof(struct wdcache_s),0L);
  p = obj_pool_alloc(entry->pool,struct wdcache_s);
  if (!p) {
    p = obj_pool_alloc_slow(entry->pool,struct wdcache_s);
  }

  if (!p)
    return NULL ;

  p->wd= wd; 
  p->fmt_id = 0;

  if (MY_RB_TREE_INSERT(&entry->u.root,p,wd,node,compare)) {
    log_error("insert wdcache %d fail\n",wd);
    //kfree(p);
    obj_pool_free(entry->pool,p);
    return NULL;
  }

  log_info("new wdcache %d\n",wd);

  entry->count++;

  return p;
}

int 
add_wdcache(wdcache_entry_t entry, const int wd, const char *pathname, int fmt_id)
{
  wdcache_t p = create_empty_wdcache(entry,wd);


  if (!p) {
    log_error("can't create wdcache %d\n", wd);
    return -1;
  }

  write_dbuf_str(p->pathname,pathname);

  p->fmt_id = fmt_id ;

  log_debug("save wdcache by: formatid: %d, path: %s\n",
            fmt_id,pathname);

  return 0;
}

static
int drop_wdcache_internal(wdcache_entry_t entry, wdcache_t p)
{
  rb_erase(&p->node,&entry->u.root);

  obj_pool_free(entry->pool,p);

  entry->count --;

  return 0;
}

int drop_wdcache(wdcache_entry_t entry, const int wd)
{
  wdcache_t p = get_wdcache(entry,wd);

  if (!p) {
    return -1;
  }

  drop_wdcache_internal(entry,p);

  return 0;
}

int init_wdcache_entry(wdcache_entry_t entry)
{
  wdcache_t pos,n;


  entry->u.root = RB_ROOT ;
  entry->count  = 0L;

  entry->pool = create_obj_pool("wdcache-pool",-1,struct wdcache_s);

  list_for_each_objPool_item(pos,n,entry->pool) {
    pos->pathname = alloc_default_dbuffer();
  }

  log_debug("done!\n");

  return 0;
}

int release_all_wdcaches(wdcache_entry_t entry)
{
  wdcache_t pos,n;

  rbtree_postorder_for_each_entry_safe(pos,n,&entry->u.root,node) {
    drop_wdcache_internal(entry,pos);
  }

  list_for_each_objPool_item(pos,n,entry->pool) {
    drop_dbuffer(pos->pathname);
  }

  release_obj_pool(entry->pool,struct wdcache_s);

  return 0;
}


size_t get_wdcache_count(wdcache_entry_t entry)
{
  return entry->count ;
}

void iterate_wdcaches(wdcache_entry_t entry)
{
  wdcache_t pos,n;

  rbtree_postorder_for_each_entry_safe(pos,n,&entry->u.root,node) {
    log_debug("wd %d, path: %s\n",pos->wd,pos->pathname);
  }
}

