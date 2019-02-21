#include <stdlib.h>
#include <string.h>
#include "order.h"
#include "kernel.h"
#include "mm_porting.h"
#include "myrbtree.h"
#include "log.h"


static const char *g_orderStatusStr[] = 
{
  "unpay",
  "paying",
  "paid ok",
  "pay error",
  "pay timeout",
} ;

char* get_pay_status_str(int st)
{
  return (char*)
         (st>=s_unpay && st<=s_err?g_orderStatusStr[st]:
         g_orderStatusStr[s_err]);
}

static int compare(char *s0, char *s1)
{
  return strcmp(s0,s1) ;
}

order_info_t get_order(order_entry_t entry, char *order_id)
{
  order_info_t p = 0;

  if (!MY_RB_TREE_FIND(&entry->u.root,order_id,p,id,node,compare)) 
    return p ;

  return NULL ;
}

order_info_t get_order_by_outTradeNo(order_entry_t entry, char *out_trade_no)
{
  order_info_t p = 0;

  if (!MY_RB_TREE_FIND(&entry->u.index_root,out_trade_no,
                       p,mch.out_trade_no,idx_node,compare)) 
    return p ;

  return NULL ;
}

int 
save_order(order_entry_t entry, char *order_id, char *mch_no, char *notify_url,
           char *out_trade_no, char *chan, char *chan_mch_no, double amount)
{
  order_info_t p = 0;

  if (!MY_RB_TREE_FIND(&entry->u.root,order_id,p,id,node,compare)) {
    return 1 ;
  }

  p = obj_pool_alloc(entry->pool,struct order_info_s);
  if (!p) {
    p = obj_pool_alloc_slow(entry->pool,struct order_info_s);
    if (p) {
      p->mch.out_trade_no = NULL;
      p->mch.notify_url = NULL;
      p->chan.name = NULL;
      p->chan.mch_no = NULL;
    }
  }

  if (!p)
    return -1 ;

  strncpy(p->id,order_id,sizeof(p->id));

  strncpy(p->mch.no,mch_no,sizeof(p->mch.no));

  if (!is_dbuffer_valid(p->mch.out_trade_no)) 
    p->mch.out_trade_no = alloc_default_dbuffer();
  write_dbuf_str(p->mch.out_trade_no,out_trade_no);

  if (!is_dbuffer_valid(p->mch.notify_url)) 
    p->mch.notify_url = alloc_default_dbuffer();
  write_dbuf_str(p->mch.notify_url,notify_url);

  if (!is_dbuffer_valid(p->chan.name)) 
    p->chan.name = alloc_default_dbuffer();
  write_dbuf_str(p->chan.name,chan);

  if (!is_dbuffer_valid(p->chan.mch_no)) 
    p->chan.mch_no = alloc_default_dbuffer();
  write_dbuf_str(p->chan.mch_no,chan_mch_no);

  p->amount = amount ;

  p->status = s_unpay;


  if (MY_RB_TREE_INSERT(&entry->u.root,p,id,node,compare)) {
    log_error("insert order id %s fail\n",order_id);
    obj_pool_free(entry->pool,p);
    return -1;
  }

  // add index by 'out_trade_no' of merchant
  if (MY_RB_TREE_INSERT(&entry->u.index_root,p,mch.out_trade_no,idx_node,compare)) {
    log_error("insert index by out trade no %s fail\n",p->mch.out_trade_no);
    obj_pool_free(entry->pool,p);
    return -1;
  }

  entry->num_orders ++;

  return 0;
}

int save_order1(order_entry_t entry, order_info_t po)
{
  return save_order(entry,po->id,po->mch.no,po->mch.notify_url,
                    po->mch.out_trade_no,po->chan.name,po->chan.mch_no,
                    po->amount);
}


static
int drop_order_internal(order_entry_t entry, order_info_t p, bool fast)
{
  rb_erase(&p->node,&entry->u.root);
  rb_erase(&p->idx_node,&entry->u.index_root);

  if (!fast) {
    drop_dbuffer(p->mch.out_trade_no);

    drop_dbuffer(p->mch.notify_url);

    drop_dbuffer(p->chan.name);

    drop_dbuffer(p->chan.mch_no);
  }

  obj_pool_free(entry->pool,p);

  entry->num_orders --;

  return 0;
}

int drop_order(order_entry_t entry, char *order_id)
{
  order_info_t p = get_order(entry,order_id);

  if (!p) {
    return -1;
  }

  drop_order_internal(entry,p,true);

  return 0;
}

int init_order_entry(order_entry_t entry, ssize_t pool_size)
{
  order_info_t pos, n;


  entry->u.root = RB_ROOT ;
  entry->u.index_root = RB_ROOT ;

  entry->num_orders = 0L;

  entry->pool = create_obj_pool("order pool",pool_size,struct order_info_s);
  list_for_each_objPool_item(pos,n,entry->pool) {
    pos->mch.out_trade_no = NULL;

    pos->mch.notify_url = NULL;

    pos->chan.name = NULL;

    pos->chan.mch_no = NULL;
  }

  log_debug("done!\n");

  return 0;
}

int release_all_orders(order_entry_t entry)
{
  order_info_t pos,n;

  rbtree_postorder_for_each_entry_safe(pos,n,&entry->u.root,node) {
    drop_order_internal(entry,pos,false);
  }

  release_obj_pool(entry->pool,struct order_info_s);

  return 0;
}

void set_order_status(order_info_t p, int st)
{
  p->status = st ;
}

int get_order_status(order_info_t p)
{
  return p->status;
}

size_t get_order_count(order_entry_t entry)
{
  return entry->num_orders ;
}

