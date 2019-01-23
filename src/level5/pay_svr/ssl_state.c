#include <stdlib.h>
#include <string.h>
#include "ssl_state.h"
#include "kernel.h"
#include "mm_porting.h"
#include "myrbtree.h"
#include "log.h"


static int compare(int fd0, int fd1)
{
  return fd0>fd1?1:fd0<fd1?-1:0 ;
}

sslstate_t get_sslstate(sslstate_entry_t entry, int fd)
{
  sslstate_t p = 0;

  if (!MY_RB_TREE_FIND(&entry->u.root,fd,p,fd,node,compare)) 
    return p ;

  return NULL ;
}

sslstate_t create_empty_sslstate(sslstate_entry_t entry, int fd)
{
  sslstate_t p = 0;

  if (!MY_RB_TREE_FIND(&entry->u.root,fd,p,fd,node,compare)) {
    log_debug("sslstate exists by fd %d\n",fd);
    return p ;
  }

  p = kmalloc(sizeof(struct sslstate_s),0L);
  if (!p)
    return NULL ;

  p->fd = fd; 
  p->out_conn = NULL;
  p->st = s_none;

  if (MY_RB_TREE_INSERT(&entry->u.root,p,fd,node,compare)) {
    log_error("insert sslstate for fd %d fail\n",fd);
    kfree(p);
    return NULL;
  }

  entry->num_sslstates++;

  return p;
}

int 
create_sslstate(sslstate_entry_t entry, int fd, connection_t pconn)
{
  sslstate_t p = create_empty_sslstate(entry,fd);

  if (!p) {
    log_error("can't create sslstate by key %d\n", fd);
    return -1;
  }

  p->fd  = fd ;
  p->out_conn  = pconn ;

  log_info("new sslstate fd %d\n",fd);

  return 0;
}

static
int drop_sslstate_internal(sslstate_entry_t entry, sslstate_t p)
{
  rb_erase(&p->node,&entry->u.root);

  kfree(p);

  entry->num_sslstates --;

  return 0;
}

int drop_sslstate(sslstate_entry_t entry, int fd)
{
  sslstate_t p = get_sslstate(entry,fd);

  if (!p) {
    return -1;
  }

  drop_sslstate_internal(entry,p);

  return 0;
}

int init_sslstate_entry(sslstate_entry_t entry)
{
  entry->u.root = RB_ROOT ;
  entry->num_sslstates = 0L;

  log_debug("done!\n");

  return 0;
}

int release_all_sslstates(sslstate_entry_t entry)
{
  sslstate_t pos,n;

  rbtree_postorder_for_each_entry_safe(pos,n,&entry->u.root,node) {
    drop_sslstate_internal(entry,pos);
  }

  return 0;
}

void update_sslstate_st(sslstate_entry_t entry, int fd, unsigned char st)
{
  sslstate_t p = get_sslstate(entry,fd);

  if (p) 
    p->st = st;
}

size_t get_sslstate_count(sslstate_entry_t entry)
{
  return entry->num_sslstates ;
}

