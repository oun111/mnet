#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include "action.h"
#include "log.h"
#include "kernel.h"
#include "mm_porting.h"
#include "myrbtree.h"


static int compare(const char *s0, const char *s1)
{
  return strcmp(s0,s1);
}

http_action_t get_http_action(http_action_entry_t entry, const char *k)
{
  http_action_t p = 0;


  if (!MY_RB_TREE_FIND(&entry->u.root,k,p,key,node,compare)) 
    return p ;

  return NULL ;
}


http_action_entry_t new_http_action_entry()
{
  http_action_entry_t entry = kmalloc(sizeof(struct http_action_entry_s),0L);


  entry->u.root = RB_ROOT ;
  entry->item_count = 0;

  return entry;
}

int 
add_http_action(http_action_entry_t entry, const char *chan, http_action_t pact)
{
  char kb[128] = "";
  http_action_t p = 0;
  char *pv = 0;


  snprintf(kb,sizeof(kb),"%s/%s",chan,pact->action);

  p = get_http_action(entry,kb);
  if (!p) {
    p = kmalloc(sizeof(struct http_action_s),0L);
    if (!p) {
      log_error("allocate new tree map item fail\n");
      return -1 ;
    }

    p->key = alloc_default_dbuffer();
    p->action  = alloc_default_dbuffer();

    write_dbuf_str(p->key,kb);

    if (MY_RB_TREE_INSERT(&entry->u.root,p,key,node,compare)) {
      log_error("insert tree map item fail\n");
      kfree(p);
      return -1;
    }

    entry->item_count++ ;
  }

  pv = pact->action ;
  write_dbuf_str(p->action,pv);

  p->cb = pact->cb;

  log_info("registering action '%s'...\n",p->key);

  return 0;
}

static
int drop_http_action_internal(http_action_entry_t entry, http_action_t p)
{
  rb_erase(&p->node,&entry->u.root);

  drop_dbuffer(p->key);
  drop_dbuffer(p->action);

  kfree(p);

  if (entry->item_count>0)
    entry->item_count-- ;

  return 0;
}

int drop_http_action(http_action_entry_t entry, http_action_t pact)
{
  http_action_t p = get_http_action(entry,pact->key);


  if (!p) {
    return -1;
  }

  drop_http_action_internal(entry,p);

  return 0;
}

static
int release_all_http_actions(http_action_entry_t entry)
{
  http_action_t pos,n;

  rbtree_postorder_for_each_entry_safe(pos,n,&entry->u.root,node) {
    drop_http_action_internal(entry,pos);
  }

  return 0;
}

void delete_http_action_entry(http_action_entry_t entry)
{
  release_all_http_actions(entry);

  kfree(entry);
}

int get_http_action_count(http_action_entry_t entry)
{
  return entry->item_count ;
}

