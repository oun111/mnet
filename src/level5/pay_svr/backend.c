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

  p = kmalloc(sizeof(struct backend_s),0L);
  if (!p)
    return NULL ;

  p->fd  = fd; 
  p->peer= NULL ;
  p->pd  = NULL ;

  if (MY_RB_TREE_INSERT(&entry->u.root,p,fd,node,compare)) {
    log_error("insert backend for fd %d fail\n",fd);
    kfree(p);
    return NULL;
  }

  entry->num_backends++;

  return p;
}

int 
create_backend(backend_entry_t entry, int fd, connection_t peer, pay_data_t pd)
{
  backend_t p = create_empty_backend(entry,fd);

  if (!p) {
    log_error("can't create backend by key %d\n", fd);
    return -1;
  }

  p->fd  = fd ;
  p->peer= peer ;
  p->pd  = pd ;
  log_info("new backend fd %d\n",fd);

  return 0;
}


static
int drop_backend_internal(backend_entry_t entry, backend_t p)
{
  rb_erase(&p->node,&entry->u.root);

  kfree(p);

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

int init_backend_entry(backend_entry_t entry)
{
  entry->u.root = RB_ROOT ;
  entry->num_backends = 0L;

  log_debug("done!\n");

  return 0;
}

int release_all_backends(backend_entry_t entry)
{
  backend_t pos,n;

  rbtree_postorder_for_each_entry_safe(pos,n,&entry->u.root,node) {
    drop_backend_internal(entry,pos);
  }

  return 0;
}


size_t get_backend_count(backend_entry_t entry)
{
  return entry->num_backends ;
}

