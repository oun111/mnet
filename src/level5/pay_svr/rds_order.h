#ifndef __RDS_ORDER_H__
#define __RDS_ORDER_H__

#include "dbuffer.h"
#include "objpool.h"
#include "myredis.h"

struct rds_order_s {

  char id[32];

  char mch_no[32];

  dbuffer_t mch_notify_url ;

  dbuffer_t mch_out_trade_no ;

  dbuffer_t chan_name ;

  dbuffer_t chan_mch_no;

  double amount ;

  int status;

  struct list_head pool_item;
} ;
typedef struct rds_order_s* rds_order_t ;


struct rds_order_entry_s {
  void *myrds_handle ;

  char cache[RDS_CACHE_NAME_SZ];
  char mq[RDS_MQ_NAME_SZ];

  objPool_t pool ;
} ;
typedef struct rds_order_entry_s* rds_order_entry_t ;



extern void init_rds_order_entry(rds_order_entry_t entry, void *myrds_handle, char *name);

extern int save_rds_order(rds_order_entry_t entry, const char *table, rds_order_t po);

extern int save_rds_order1(rds_order_entry_t entry, const char *table, char *id, char *mch_no,
                           char *mch_notify_url, char *mch_sid, char *chan_name, char *chan_mch_no,
                           double amount, int status);

extern rds_order_t get_rds_order(rds_order_entry_t entry, const char *table, const char *orderid);

extern int drop_rds_order(rds_order_entry_t entry, rds_order_t p);

extern int release_all_rds_orders(rds_order_entry_t entry);


#endif /* __RDS_ORDER_H__*/
