#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include "pay_action.h"
#include "log.h"
#include "kernel.h"
#include "mm_porting.h"
#include "myrbtree.h"


static int compare(const char *s0, const char *s1)
{
  return strcmp(s0,s1);
}

pay_action_t get_pay_action(pay_action_entry_t entry, const char *k)
{
  pay_action_t p = 0;


  if (!MY_RB_TREE_FIND(&entry->u.root,k,p,key,node,compare)) 
    return p ;

  return NULL ;
}


pay_action_entry_t new_pay_action_entry()
{
  pay_action_entry_t entry = kmalloc(sizeof(struct pay_action_entry_s),0L);


  entry->u.root = RB_ROOT ;
  entry->item_count = 0;

  return entry;
}

int 
add_pay_action(pay_action_entry_t entry, pay_action_t pact)
{
  pay_action_t p = get_pay_action(entry,pact->key);
  char *pv = 0;
  size_t ln = 0L ;


  if (!p) {
    p = kmalloc(sizeof(struct pay_action_s),0L);
    if (!p) {
      log_error("allocate new tree map item fail\n");
      return -1 ;
    }

    p->key = alloc_default_dbuffer();
    p->action  = alloc_default_dbuffer();
    p->channel = alloc_default_dbuffer();

    pv = pact->key ;
    ln = strlen(pact->key);
    p->key= write_dbuffer(p->key,pv,ln);
    p->key[ln] = '\0';

    if (MY_RB_TREE_INSERT(&entry->u.root,p,key,node,compare)) {
      log_error("insert tree map item fail\n");
      kfree(p);
      return -1;
    }

    entry->item_count++ ;
  }

  pv = pact->action ;
  ln = strlen(pact->action);
  p->action= write_dbuffer(p->action,pv,ln);
  p->action[ln] = '\0';

  pv = pact->channel ;
  ln = strlen(pact->channel);
  p->channel= write_dbuffer(p->action,pv,ln);
  p->channel[ln] = '\0';

  p->cb = pact->cb;

  return 0;
}

static
int drop_pay_action_internal(pay_action_entry_t entry, pay_action_t p)
{
  rb_erase(&p->node,&entry->u.root);

  drop_dbuffer(p->key);
  drop_dbuffer(p->action);
  drop_dbuffer(p->channel);

  kfree(p);

  if (entry->item_count>0)
    entry->item_count-- ;

  return 0;
}

int drop_pay_action(pay_action_entry_t entry, pay_action_t pact)
{
  pay_action_t p = get_pay_action(entry,pact->key);


  if (!p) {
    return -1;
  }

  drop_pay_action_internal(entry,p);

  return 0;
}

static
int release_all_pay_actions(pay_action_entry_t entry)
{
  pay_action_t pos,n;

  rbtree_postorder_for_each_entry_safe(pos,n,&entry->u.root,node) {
    drop_pay_action_internal(entry,pos);
  }

  return 0;
}

void delete_pay_action_entry(pay_action_entry_t entry)
{
  release_all_pay_actions(entry);

  kfree(entry);
}

int get_pay_action_count(pay_action_entry_t entry)
{
  return entry->item_count ;
}

