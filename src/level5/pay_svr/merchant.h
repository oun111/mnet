#ifndef __MERCHANT_H__
#define __MERCHANT_H__


#include "dbuffer.h"
#include "rbtree.h"
#include "objpool.h"


#define MCH_ID_SIZE  32


struct fixed_amount_s {
  struct rb_node node ;
  double amount ;
} ;
typedef struct fixed_amount_s* fixed_amt ;

struct merchant_info_s {

  char id[MCH_ID_SIZE];

  tree_map_t mch_info ;

  // frequently use params
  dbuffer_t pubkey;
  dbuffer_t privkey ;
  bool verify_sign ;
  int sign_type ;

  struct {
    int method:4; // 0: range, 1: fixed
    double max; // max amount per request
    double min;// min amount per request
    struct rb_root fixed ; // fix amounts
  } amounts ;

  // trade time
  struct {
    int t0;
    int t1 ;
  } tt; 

  struct rb_node node ;

  // the merchant-based pay route list
  struct list_head alipay_pay_route ;

  // the merchant-based transfund route list
  struct list_head alipay_transfund_route ;
} ;
typedef struct merchant_info_s* merchant_info_t ;


struct merchant_entry_s {
  union {
    struct rb_root root;
  } u;

  size_t num_merchants ;
} ;
typedef struct merchant_entry_s* merchant_entry_t ;



extern merchant_info_t get_merchant(merchant_entry_t entry, char *merchant_id);

extern bool is_merchant_amount_valid(merchant_info_t pm, double amt, dbuffer_t *errbuf);

extern int save_merchant(merchant_entry_t entry, char *merchant_id, tree_map_t mch_info) ;

extern int drop_merchant(merchant_entry_t entry, char *merchant_id);

extern int init_merchant_entry(merchant_entry_t entry, ssize_t pool_size);

extern int release_all_merchants(merchant_entry_t entry);

extern size_t get_merchant_count(merchant_entry_t entry);

extern int init_merchant_data(merchant_entry_t pm);

#endif /* __MERCHANT_H__*/
