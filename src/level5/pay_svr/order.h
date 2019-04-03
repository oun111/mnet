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

enum user_notify_status {
  s_not_notify,
  s_notifying,
  s_notify_ok,
  s_notify_fail,
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

    dbuffer_t message;
  } chan ;

  double amount ;

  int status; // order status

  int un_status; // user notify status

  long long create_time;

  struct rb_node node ;

  struct rb_node idx_node ;

  struct list_head pool_item;

  struct list_head cached_item;
} ;
typedef struct order_info_s* order_info_t ;


struct order_entry_s {
  struct {
    struct rb_root root;
    struct rb_root index_root;
  } u;

  size_t num_orders ;

  objPool_t pool ;

  struct list_head cached_orders;
} ;
typedef struct order_entry_s* order_entry_t ;


extern order_info_t get_order(order_entry_t entry, char *order_id);

extern order_info_t get_order_by_outTradeNo(order_entry_t entry, char *out_trade_no);

extern order_info_t save_order(order_entry_t entry, char *order_id, char *mch_no, 
                      char *notify_url, char *out_trade_no, char *chan, 
                      char *chan_mch_no, double amount) ;

extern order_info_t save_order1(order_entry_t entry, order_info_t po);

extern int drop_order(order_entry_t entry, char *order_id);

extern int init_order_entry(order_entry_t entry, ssize_t pool_size);

extern int release_all_orders(order_entry_t entry);

extern size_t get_order_count(order_entry_t entry);

extern void set_order_status(order_info_t p, int st);

extern int get_order_status(order_info_t p);

extern void set_order_un_status(order_info_t p, int st);

extern char* get_pay_status_str(int st);

extern void set_order_message(order_info_t p, const char *msg);

#endif /* __ORDER_H__*/
