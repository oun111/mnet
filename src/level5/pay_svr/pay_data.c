#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include "pay_data.h"
#include "log.h"
#include "kernel.h"
#include "mm_porting.h"
#include "myrbtree.h"


static int compare(const char *s0, const char *s1)
{
  return strcmp(s0,s1);
}

pay_channel_t get_pay_channel(pay_channels_entry_t entry, const char *chan)
{
  pay_channel_t p = 0;


  if (MY_RB_TREE_FIND(&entry->u.root,chan,p,channel,node,compare)) 
    return NULL ;

  return p;
}

pay_data_t get_pay_data(pay_channels_entry_t entry, const char *channel, 
                             const char *subname)
{
  pay_channel_t p = get_pay_channel(entry,channel);
  pay_data_t pd = 0;


  if (!p)
    return NULL ;

  list_for_each_entry(pd,&p->pay_data_list,upper) {
    if (!strcmp(pd->subname,subname))
      return pd;
  }

  return NULL ;
}


pay_channels_entry_t new_pay_channels_entry()
{
  pay_channels_entry_t entry = kmalloc(sizeof(struct pay_channels_entry_s),0L);


  entry->u.root = RB_ROOT ;

  return entry;
}

pay_channel_t new_pay_channel(const char *chan)
{
  size_t vl = 0L;
  char *pv  = NULL;
  pay_channel_t pc = kmalloc(sizeof(struct pay_channel_item_s),0L);


  pv = (char*)chan ;
  vl = strlen(chan);
  pc->channel = alloc_default_dbuffer();
  pc->channel = write_dbuffer(pc->channel,pv,vl);
  pc->channel[vl] = '\0';
  INIT_LIST_HEAD(&pc->pay_data_list);

  return pc ;
}

pay_data_t 
add_pay_data(pay_channels_entry_t entry, const char *chan, 
             const char *subname, tree_map_t params)
{
  char *pv = 0;
  size_t ln = 0L ;
  pay_data_t p = get_pay_data(entry,chan,subname);


  if (!p) {
    pay_channel_t pc = NULL ;


    p = kmalloc(sizeof(struct pay_data_item_s),0L);
    if (!p) {
      log_error("allocate new tree map item fail\n");
      return NULL ;
    }

    p->subname = alloc_default_dbuffer(chan);
    p->freq   = 0;
    p->weight = 0;
    p->pay_params = /*new_tree_map()*/params;
    INIT_LIST_HEAD(&p->upper); 


    pc = new_pay_channel(chan);
    list_add(&p->upper,&pc->pay_data_list);


    if (MY_RB_TREE_INSERT(&entry->u.root,pc,channel,node,compare)) {
      log_error("insert pay data item fail\n");
      kfree(p);
      return NULL;
    }
  }

  pv = (char*)subname ;
  ln = strlen(subname);
  p->subname= write_dbuffer(p->subname,pv,ln);
  p->subname[ln] = '\0';

  return p;
}

static
int drop_pay_data_internal(pay_channel_t pc)
{
  pay_data_t pos,n ;


  list_for_each_entry_safe(pos,n,&pc->pay_data_list,upper) {
    drop_dbuffer(pos->subname);
    kfree(pos);
  }

  return 0;
}

int drop_pay_channel(pay_channels_entry_t entry, const char *chan)
{
  pay_channel_t pc = get_pay_channel(entry,chan);


  if (!pc) {
    return -1;
  }

  drop_pay_data_internal(pc);

  rb_erase(&pc->node,&entry->u.root);

  drop_dbuffer(pc->channel);
  kfree(pc);

  return 0;
}

static
int release_all_pay_datas(pay_channels_entry_t entry)
{
  pay_channel_t pos,n;


  rbtree_postorder_for_each_entry_safe(pos,n,&entry->u.root,node) {
    drop_pay_channel(entry,pos->channel);
  }

  return 0;
}

void delete_pay_channels_entry(pay_channels_entry_t entry)
{
  release_all_pay_datas(entry);

  kfree(entry);
}

pay_data_t get_pay_route(pay_channels_entry_t entry, const char *chan)
{
  pay_channel_t pc = get_pay_channel(entry,chan);
  pay_data_t pos ;

  
  if (!pc) {
    return NULL ;
  }

  // TODO: get best pay route
  list_for_each_entry(pos,&pc->pay_data_list,upper) {
    return pos ;
  }

  return NULL ;
}

