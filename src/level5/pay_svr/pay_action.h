#ifndef __PAY_ACTION_H__
#define __PAY_ACTION_H__

#include "connection.h"
#include "rbtree.h"
#include "tree_map.h"


struct pay_action_s {
  dbuffer_t key ;
  dbuffer_t channel;
  dbuffer_t action;
  int (*cb)(Network_t,connection_t,tree_map_t);
  void (*reg_modules)();
  struct rb_node node ;
} ;
typedef struct pay_action_s* pay_action_t;



struct pay_action_entry_s {
  union {
    struct rb_root root;
  } u;

  int item_count ;
} ;
typedef struct pay_action_entry_s* pay_action_entry_t ;



extern pay_action_entry_t new_pay_action_entry();

extern pay_action_t get_pay_action(pay_action_entry_t entry, const char *k);

extern int add_pay_action(pay_action_entry_t entry, pay_action_t pact);

extern int drop_pay_action(pay_action_entry_t entry, pay_action_t pact);

extern void delete_pay_action_entry(pay_action_entry_t entry);

extern int get_pay_action_count(pay_action_entry_t entry);

extern void register_pay_action_modules(pay_action_entry_t entry);

#endif /* __PAY_ACTION_H__*/
