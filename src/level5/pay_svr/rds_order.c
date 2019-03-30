#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "rds_order.h"
#include "myredis.h"
#include "jsons.h"
#include "tree_map.h"
#include "log.h"


void 
init_rds_order_entry(rds_order_entry_t entry, void *myrds_handle, char *name)
{
  rds_order_t pos,n;


  entry->myrds_handle = myrds_handle ;
  snprintf(entry->cache,sizeof(entry->cache),"%s",name);
  snprintf(entry->mq,sizeof(entry->mq),"%s_mq",name);

  entry->pool = create_obj_pool("rds_order pool",-1,struct order_info_s);

  list_for_each_objPool_item(pos,n,entry->pool) {
    pos->mch.out_trade_no = alloc_default_dbuffer();

    pos->mch.notify_url = alloc_default_dbuffer();

    pos->chan.name = alloc_default_dbuffer();

    pos->chan.mch_no = alloc_default_dbuffer();
  }
}

int save_rds_order1(rds_order_entry_t entry, const char *table, char *id, 
                    char *mch_no, char *mch_notify_url, char *mch_sid,   
                    char *chan_name, char *chan_mch_no, double amount, 
                    int status, int un_status, long long create_time,
                    char *chan_message)
{
  tree_map_t map = new_tree_map();
  char tmp[64]="";
  dbuffer_t str = alloc_default_dbuffer();
  int ret = 0;


  put_tree_map_string(map,"orderid",id);
  put_tree_map_string(map,"mch_no",mch_no);
  put_tree_map_string(map,"mch_notify_url",mch_notify_url);
  put_tree_map_string(map,"mch_orderid",mch_sid);
  put_tree_map_string(map,"chan_name",chan_name);
  put_tree_map_string(map,"chan_mch_no",chan_mch_no);
  put_tree_map_string(map,"chan_message",chan_message);

  snprintf(tmp,sizeof(tmp),"$%f",amount);
  put_tree_map_string(map,"amount",tmp);

  snprintf(tmp,sizeof(tmp),"$%d",status);
  put_tree_map_string(map,"status",tmp);

  snprintf(tmp,sizeof(tmp),"$%d",un_status);
  put_tree_map_string(map,"user_notify_status",tmp);

  snprintf(tmp,sizeof(tmp),"$%lld",create_time);
  put_tree_map_string(map,"create_time",tmp);

  jsonKV_t *pr = jsons_parse_tree_map(map);
  pr = jsons_add_to_array(NULL,pr);
  jsons_toString(pr,&str,true);
  jsons_release(pr);

  // write to redis: key: orderid, value: the whole order
  if (myredis_write((myredis_t)entry,table,id,str,mr__need_sync)) {
    log_error("write failed\n");
    ret = -1;
  }

  // save index: key: merchant out-trade-no, value: orderid
  if (myredis_write((myredis_t)entry,table,mch_sid,id,mr__ok)) {
    log_error("write index failed\n");
    ret = -1;
  }

  drop_dbuffer(str);
  delete_tree_map(map);

  //return ret;

  (void)ret;
  return 0;
}

int save_rds_order(rds_order_entry_t entry, const char *table, rds_order_t po)
{
  return save_rds_order1(entry,table,po->id,po->mch.no,po->mch.notify_url,
                         po->mch.out_trade_no,po->chan.name,po->chan.mch_no,
                         po->amount,po->status,po->un_status,po->create_time,
                         po->chan.message);
}

int get_rds_order_index(rds_order_entry_t entry, const char *table, 
                        const char *out_trade_no, dbuffer_t *orderid)
{
  int rc = 1;


  for (int i=0;/*i<10 &&*/ rc==1; i++) {
    rc = myredis_read((myredis_t)entry,table,out_trade_no,orderid);
  }

  // get nothing
  if (rc<0 || rc==1) {
    log_error("get nothing, rc=%d\n",rc);
    return -1;
  }

  return 0;
}

rds_order_t 
get_rds_order(rds_order_entry_t entry, const char *table, 
              const char *orderid, bool fast)
{
  rds_order_t p = 0;
  char *tmp = 0;
  dbuffer_t str = alloc_default_dbuffer();
  int rc = 1;
  jsonKV_t *pr = 0;
  tree_map_t map = 0, odr_map = 0;
  tm_item_t pos,n,pos1,n1;


  for (int i=0;/*i<10 &&*/ rc==1; i++) {
    rc = myredis_read((myredis_t)entry,table,orderid,&str);
  }

  // get nothing
  if (rc<0 || rc==1) {
    log_error("get nothing, rc=%d\n",rc);
    goto __done;
  }

  // allocate space
  p = obj_pool_alloc(entry->pool,struct order_info_s);
  if (!p) {
    p = obj_pool_alloc_slow(entry->pool,struct order_info_s);
    if (p) {
      p->mch.out_trade_no = alloc_default_dbuffer();
      p->mch.notify_url   = alloc_default_dbuffer();
      p->chan.name   = alloc_default_dbuffer();
      p->chan.mch_no = alloc_default_dbuffer();
      p->chan.message= alloc_default_dbuffer();
    }
  }

  if (fast) {
    goto __done ;
  }

  // got it
  pr  = jsons_parse(str);
  map = jsons_to_treemap(pr);

  // we only take the 1st order
  rbtree_postorder_for_each_entry_safe(pos,n,&map->u.root,node) {
    tree_map_t mp1 = pos->nest_map;
    rbtree_postorder_for_each_entry_safe(pos1,n1,&mp1->u.root,node) {
      odr_map = pos1->nest_map;
      break ;
    }
  }

  tmp = get_tree_map_value(odr_map,"mch_no");
  strncpy(p->mch.no,tmp,sizeof(p->mch.no));

  tmp = get_tree_map_value(odr_map,"mch_notify_url");
  write_dbuf_str(p->mch.notify_url,tmp);

  tmp = get_tree_map_value(odr_map,"mch_orderid");
  write_dbuf_str(p->mch.out_trade_no,tmp);

  tmp = get_tree_map_value(odr_map,"chan_name");
  write_dbuf_str(p->chan.name,tmp);

  tmp = get_tree_map_value(odr_map,"chan_mch_no");
  write_dbuf_str(p->chan.mch_no,tmp);

  tmp = get_tree_map_value(odr_map,"chan_message");
  write_dbuf_str(p->chan.message,tmp);

  tmp = get_tree_map_value(odr_map,"orderid");
  strcpy(p->id,tmp);

  tmp = get_tree_map_value(odr_map,"amount");
  p->amount = atof(tmp);

  tmp = get_tree_map_value(odr_map,"status");
  p->status = atoi(tmp);

  tmp = get_tree_map_value(odr_map,"user_notify_status");
  p->un_status = atoi(tmp);

  tmp = get_tree_map_value(odr_map,"create_time");
  p->create_time = atoll(tmp);

  jsons_release(pr);

__done:
  drop_dbuffer(str);

  return p;
}

bool is_rds_order_exist(rds_order_entry_t entry, const char *table, 
                        const char *orderid)
{
  rds_order_t p = get_rds_order(entry,table,orderid,true);


  if (p) {
    release_rds_order(entry,p);
    return true ;
  }

  return false ;
}

bool is_rds_outTradeNo_exist(rds_order_entry_t entry, const char *table, 
                               const char *out_trade_no)
{
  dbuffer_t orderid = alloc_default_dbuffer();
  int ret = get_rds_order_index(entry,table,out_trade_no,&orderid);


  drop_dbuffer(orderid);
  return !ret ;
}

static
int drop_rds_order_internal(rds_order_entry_t entry, rds_order_t p, bool fast)
{
  if (!fast) {
    drop_dbuffer(p->mch.out_trade_no);

    drop_dbuffer(p->mch.notify_url);

    drop_dbuffer(p->chan.name);

    drop_dbuffer(p->chan.mch_no);

    drop_dbuffer(p->chan.message);
  }

  obj_pool_free(entry->pool,p);

  return 0;
}

int drop_rds_order(rds_order_entry_t entry, rds_order_t p)
{
  return drop_rds_order_internal(entry,p,false);
}

int release_rds_order(rds_order_entry_t entry, rds_order_t p)
{
  return drop_rds_order_internal(entry,p,true);
}

int release_all_rds_orders(rds_order_entry_t entry)
{
  rds_order_t pos,n;

  list_for_each_objPool_item(pos,n,entry->pool) {
    drop_rds_order_internal(entry,pos,false);
  }

  release_obj_pool(entry->pool,struct order_info_s);

  return 0;
}
