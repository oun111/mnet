#ifndef __PAY_DATA_H__
#define __PAY_DATA_H__

#include "dbuffer.h"
#include "tree_map.h"
#include "list.h"


#define REQ_URL      "req_url"
#define PARAM_TYPE   "param_type"
#define PAY_DATA     "pay_data"
#define CRYPTO       "crypto"
#define PUBKEY       "public_key_path"
#define PRIVKEY      "private_key_path"


struct pay_data_item_s {
  dbuffer_t subname ;

  int weight ;

  int freq ;

  tree_map_t pay_params ;

  struct list_head upper ;
} ;
typedef struct pay_data_item_s* pay_data_t ;


struct pay_channel_item_s {
  dbuffer_t channel ;

  struct list_head pay_data_list ;

  struct rb_node node ;
} ;
typedef struct pay_channel_item_s* pay_channel_t ;


struct pay_channels_entry_s {

  union {
    struct rb_root root;
  } u;
} ;
typedef struct pay_channels_entry_s* pay_channels_entry_t ;



extern pay_channel_t get_pay_channel(pay_channels_entry_t entry, const char *chan);

extern pay_data_t get_pay_data(pay_channel_t pc, const char *subname);

extern pay_channels_entry_t new_pay_channels_entry();

extern pay_data_t add_pay_data(pay_channels_entry_t entry, const char *chan, 
                               const char *subname, tree_map_t params);

extern void delete_pay_channels_entry(pay_channels_entry_t entry);

extern pay_data_t get_pay_route(pay_channels_entry_t entry, const char *chan);

extern int init_pay_data(pay_channels_entry_t *paych);

#endif /* __PAY_DATA_H__*/
