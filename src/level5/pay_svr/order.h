#ifndef __ORDER_H__
#define __ORDER_H__


#include "dbuffer.h"
#include "rbtree.h"
#include "objpool.h"


#define ODR_ID_SIZE  32


enum order_status {
  s_unpay = 0,
  s_paying= 1,
  s_paid  = 2,
  s_err   = 3,
  s_timeout = 4,
} ;

struct order_info_s {

  char id[ODR_ID_SIZE];

  struct merchant_s {
    char no[32];

    dbuffer_t notify_url ;

    dbuffer_t out_trade_no ;
  } mch ;

  struct channel_s {
    dbuffer_t name ;

    dbuffer_t mch_no;
  } chan ;

  double amount ;

  int status;

  struct rb_node node ;

  struct list_head pool_item;
} ;
typedef struct order_info_s* order_info_t ;


struct order_entry_s {
  union {
    struct rb_root root;
  } u;

  size_t num_orders ;

  objPool_t pool ;
} ;
typedef struct order_entry_s* order_entry_t ;


extern order_info_t get_order(order_entry_t entry, char *order_id);

extern int save_order(order_entry_t entry, char *order_id, char *mch_no, 
                      char *notify_url, char *out_trade_no, char *chan, 
                      char *chan_mch_no, double amount) ;

extern int save_order1(order_entry_t entry, order_info_t po);

extern int drop_order(order_entry_t entry, char *order_id);

extern int init_order_entry(order_entry_t entry, ssize_t pool_size);

extern int release_all_orders(order_entry_t entry);

extern size_t get_order_count(order_entry_t entry);

extern void set_order_status(order_info_t p, int st);

extern int get_order_status(order_info_t p);

extern char* get_pay_status_str(int st);

#endif /* __ORDER_H__*/
