#include <stdlib.h>
#include <pthread.h>
#include "sock_buffer.h"
#include "log.h"
#include "kernel.h"
#include "mm_porting.h"
#include "myrbtree.h"


static int compare(int fd0, int fd1)
{
  return fd0>fd1?1:fd0<fd1?-1:0;
}

sbuf_t get_sock_buffer(sbuf_entry_t entry, int fd)
{
  sbuf_t p = 0;


  if (!MY_RB_TREE_FIND(&entry->u.root,fd,p,fd,node,compare)) 
    return p ;

  return NULL ;
}

int init_sock_buffer_entry(sbuf_entry_t entry)
{
  entry->u.root = RB_ROOT ;
  entry->sbuf_count = 0;

  INIT_LIST_HEAD(&entry->qhead);

  log_debug("done!\n");

  return 0;
}

sbuf_t 
create_sock_buffer(sbuf_entry_t entry, int fd, void *net, void *conn)
{
  sbuf_t p = 0;


  MY_RB_TREE_FIND(&entry->u.root,fd,p,fd,node,compare);

  if (!p) {
    p = kmalloc(sizeof(struct sock_buffer_s),0L);
    if (!p) {
      log_error("allocate new sock buffer for "
                "fd %d fail\n",fd);
      return NULL ;
    }

    p->in  = alloc_default_dbuffer();
    p->out = alloc_default_dbuffer();
    p->net = net;
    p->conn= conn ;
  }

  p->fd = fd; 

  if (MY_RB_TREE_INSERT(&entry->u.root,p,fd,node,compare)) {
    log_error("insert sock buffer for fd %d fail\n",fd);
    kfree(p);
    return NULL;
  }

  pthread_mutex_init(&p->mtx,NULL);
  pthread_cond_init(&p->cond,NULL);

  p->valid = 1;

  entry->sbuf_count++ ;

  return p;
}

static
int drop_sock_buffer_internal(sbuf_entry_t entry, sbuf_t p)
{
  rb_erase(&p->node,&entry->u.root);

  if (p->qitem.next!=LIST_POISON1 && 
      p->qitem.prev!=LIST_POISON2) {
    list_del(&p->qitem);
  }

  drop_dbuffer(p->in);
  drop_dbuffer(p->out);

  pthread_mutex_destroy(&p->mtx);
  pthread_cond_destroy(&p->cond);

  kfree(p);

  if (entry->sbuf_count>0)
    entry->sbuf_count-- ;

  return 0;
}

int drop_sock_buffer(sbuf_entry_t entry, int fd)
{
  sbuf_t p = get_sock_buffer(entry,fd);


  if (!p) {
    return -1;
  }

  drop_sock_buffer_internal(entry,p);

  return 0;
}

int release_all_sock_buffers(sbuf_entry_t entry)
{
  sbuf_t pos,n;

  rbtree_postorder_for_each_entry_safe(pos,n,&entry->u.root,node) {
    drop_sock_buffer_internal(entry,pos);
  }

  return 0;
}

int get_sock_buffers_count(sbuf_entry_t entry)
{
  return entry->sbuf_count ;
}

void set_sock_buffer_valid(sbuf_t psb, int v)
{
  psb->valid = v;
}

int is_sock_buffer_valid(sbuf_t psb)
{
  return psb && psb->valid ;
}


/*
 * sock buffer queue
 */
int sbufq_pop(sbuf_entry_t entry)
{
  sbuf_t p = list_first_entry_or_null(&entry->qhead,struct sock_buffer_s,qitem);


  if (!p)
    return -1;

  list_del(&p->qitem);

  return p->fd;
}

void sbufq_push(sbuf_entry_t entry, sbuf_t p)
{
  // add to sock buffer queue
  list_add_tail(&p->qitem,&entry->qhead);
}

