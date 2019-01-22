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
  //if (!MY_RB_TREE_FIND(&entry->u.root,fd,p,fd,node)) 
    return p ;

  return NULL ;
}

backend_t create_empty_backend(backend_entry_t entry, int fd)
{
  backend_t p = 0;

  if (!MY_RB_TREE_FIND(&entry->u.root,fd,p,fd,node,compare)) {
  //if (!MY_RB_TREE_FIND(&entry->u.root,fd,p,fd,node)) {
    log_debug("backend exists by fd %d\n",fd);
    return p ;
  }

  p = kmalloc(sizeof(struct backend_s),0L);
  if (!p)
    return NULL ;

  p->fd = fd; 
  p->no = -1;
  p->task = NULL ;
  p->is_use = 0;
  p->addr[0] = '\0';
  p->port = -1;
  p->schema= alloc_default_dbuffer();
  p->usr= alloc_default_dbuffer();
  p->pwd= alloc_default_dbuffer();

  p->status = st_empty;

  p->eof_count = 0;

  p->conn     = NULL ;

  if (MY_RB_TREE_INSERT(&entry->u.root,p,fd,node,compare)) {
  //if (MY_RB_TREE_INSERT(&entry->u.root,p,fd,node)) {
    log_error("insert backend for fd %d fail\n",fd);
    kfree(p);
    return NULL;
  }

  entry->num_backends++;

  return p;
}

int 
create_backend(backend_entry_t entry, int fd, int no, const char *addr, 
               int port, const char* sch, const char* usr, const char *pwd)
{
  backend_t p = create_empty_backend(entry,fd);

  if (!p) {
    log_error("can't create backend by key %d\n", fd);
    return -1;
  }

  p->fd  = fd ;
  p->no  = no ;

  p->schema = write_dbuffer(p->schema,(char*)sch,strlen(sch));
  p->schema[strlen(sch)] = '\0';
  p->usr = write_dbuffer(p->usr,(char*)usr,strlen(usr));
  p->pwd = write_dbuffer(p->pwd,(char*)pwd,strlen(pwd));

  strncpy(p->addr,addr,sizeof(p->addr)) ;
  p->port = port ;

  log_info("new backend fd %d\n",fd);

  return 0;
}

int set_backend_state(backend_t p, int st)
{
  p->status = st;

  return 0;
}

backend_t 
find_backend_by_num(backend_entry_t entry, int num, int *in_use)
{
  backend_t pos,n;
  int found = 0;

  rbtree_postorder_for_each_entry_safe(pos,n,&entry->u.root,node) {
    if (pos->no==num) {
      found = 1;
      if (pos->is_use!=1) 
        return pos ;
    }
  }

  if (found)
    *in_use = 1;

  return NULL ;
}

void set_backend_unuse(backend_t p)
{
  p->is_use = 0;
  p->task = NULL ;
}

void set_backend_usage(backend_t p, task_t tsk)
{
  p->is_use = 1;
  p->task = tsk ;
  p->eof_count = 0;
}

static
int drop_backend_internal(backend_entry_t entry, backend_t p)
{
  rb_erase(&p->node,&entry->u.root);

  drop_dbuffer(p->usr);
  drop_dbuffer(p->pwd);
  drop_dbuffer(p->schema);

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

void show_backend_in_use(backend_entry_t entry)
{
  backend_t pos,n;
  int in_use = 0;

  for_each_backends(entry,pos,n) {
    if (pos->is_use==1) 
      in_use = 1;
  }
  if (in_use==0)
    log_debug("all backends' freed2222\n");
  else
    log_debug("some backend(s) still in use!!!!!!!!!!\n");
}

size_t get_backend_count(backend_entry_t entry)
{
  return entry->num_backends ;
}

void 
get_all_backend_num(backend_entry_t entry, int bk_num_list[], 
                    int bk_count)
{
  backend_t pos,n;
  int i = 0;

  for_each_backends(entry,pos,n) {
    if (i>=bk_count)
      break ;

    bk_num_list[i++] = pos->no ;
  }
}

