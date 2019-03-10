#ifndef __RDS_ORDER_H__
#define __RDS_ORDER_H__

#include "dbuffer.h"
#include "objpool.h"
#include "myredis.h"
#include "order.h"


typedef order_info_t rds_order_t ;

struct rds_order_entry_s {
  void *myrds_handle ;

  char cache[RDS_CACHE_NAME_SZ];
  char mq[RDS_MQ_NAME_SZ];

  objPool_t pool ;
} ;
typedef struct rds_order_entry_s* rds_order_entry_t ;



extern void init_rds_order_entry(rds_order_entry_t entry, void *myrds_handle, 
                                 char *name);

extern int save_rds_order(rds_order_entry_t entry, const char *table, 
                          rds_order_t po);

extern int save_rds_order1(rds_order_entry_t entry, const char *table, char *id, 
                           char *mch_no, char *mch_notify_url, char *mch_sid, 
                           char *chan_name, char *chan_mch_no, double amount, 
                           int status);

extern rds_order_t get_rds_order(rds_order_entry_t entry, const char *table, 
                                 const char *orderid, bool fast);

extern int drop_rds_order(rds_order_entry_t entry, rds_order_t p);

extern int release_rds_order(rds_order_entry_t entry, rds_order_t p);

extern int release_all_rds_orders(rds_order_entry_t entry);

extern bool is_rds_order_exist(rds_order_entry_t entry, const char *table, 
                               const char *orderid) ;

extern int get_rds_order_index(rds_order_entry_t entry, const char *table, 
                               const char *out_trade_no, dbuffer_t *orderid);

#endif /* __RDS_ORDER_H__*/
