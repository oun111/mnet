#include <stdlib.h>
#include <string.h>
#include "backend.h"
#include "kernel.h"
#include "mm_porting.h"
#include "myrbtree.h"
#include "log.h"


static int compare(int fd0, int fd1)
{
  return fd0>fd1?1:fd0<fd1?-1:0 ;
}

backend_t get_backend(backend_entry_t entry, int fd)
{
  backend_t p = 0;

  if (!MY_RB_TREE_FIND(&entry->u.root,fd,p,fd,node,compare)) 
    return p ;

  return NULL ;
}

backend_t create_empty_backend(backend_entry_t entry, int fd)
{
  backend_t p = 0;

  if (!MY_RB_TREE_FIND(&entry->u.root,fd,p,fd,node,compare)) {
    log_debug("backend exists by fd %d\n",fd);
    return p ;
  }

  //p = kmalloc(sizeof(struct backend_s),0L);
  p = obj_pool_alloc(entry->pool,struct backend_s);
  if (!p) {
    p = obj_pool_alloc_slow(entry->pool,struct backend_s);
  }

  if (!p)
    return NULL ;

  p->fd  = fd; 
  p->peer= NULL ;
  p->data= alloc_default_dbuffer() ;
  p->type= 0;

  if (MY_RB_TREE_INSERT(&entry->u.root,p,fd,node,compare)) {
    log_error("insert backend for fd %d fail\n",fd);
    //kfree(p);
    obj_pool_free(entry->pool,p);
    return NULL;
  }

  entry->num_backends++;

  return p;
}

int 
create_backend(backend_entry_t entry, int fd, connection_t peer, 
               int t, void *d)
{
  backend_t p = create_empty_backend(entry,fd);

  if (!p) {
    log_error("can't create backend by key %d\n", fd);
    return -1;
  }

  p->fd  = fd ;
  p->peer= peer ;
  //strncpy(p->data,(char*)d,sizeof(p->data)) ;
  write_dbuf_str(p->data,d);
  p->type= t ;
  log_info("new backend fd %d\n",fd);

  return 0;
}


static
int drop_backend_internal(backend_entry_t entry, backend_t p)
{
  rb_erase(&p->node,&entry->u.root);

  if (is_dbuffer_valid(p->data)) {
    drop_dbuffer(p->data);
  }

  obj_pool_free(entry->pool,p);

  entry->num_backends --;

  return 0;
}

int drop_backend(backend_entry_t entry, int fd)
{
  backend_t p = get_backend(entry,fd);

  if (!p) {
    return -1;
  }

  drop_backend_internal(entry,p);

  return 0;
}

int init_backend_entry(backend_entry_t entry, ssize_t pool_size)
{
  entry->u.root = RB_ROOT ;
  entry->num_backends = 0L;

  entry->pool = create_obj_pool("paysvr-backend-pool",pool_size,struct backend_s);

  log_debug("done!\n");

  return 0;
}

int release_all_backends(backend_entry_t entry)
{
  backend_t pos,n;

  //rbtree_postorder_for_each_entry_safe(pos,n,&entry->u.root,node) {
  MY_RBTREE_PREORDER_FOR_EACH_ENTRY_SAFE(pos,n,&entry->u.root,node) {
    drop_backend_internal(entry,pos);
  }

  release_obj_pool(entry->pool,struct backend_s);

  return 0;
}


size_t get_backend_count(backend_entry_t entry)
{
  return entry->num_backends ;
}

