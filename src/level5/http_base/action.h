#ifndef __ACTION_H__
#define __ACTION_H__

#include "connection.h"
#include "rbtree.h"
#include "tree_map.h"


struct http_action_s {
  dbuffer_t key ;
  dbuffer_t action;
  int (*cb)(Network_t,connection_t,tree_map_t);
  struct rb_node node ;
} ;
typedef struct http_action_s* http_action_t;



struct http_action_entry_s {
  union {
    struct rb_root root;
  } u;

  int item_count ;
} ;
typedef struct http_action_entry_s* http_action_entry_t ;



extern http_action_entry_t new_http_action_entry();

extern http_action_t get_http_action(http_action_entry_t entry, const char *k);

extern int add_http_action(http_action_entry_t entry, const char *chan, http_action_t pact);

extern int drop_http_action(http_action_entry_t entry, http_action_t pact);

extern void delete_http_action_entry(http_action_entry_t entry);

extern int get_http_action_count(http_action_entry_t entry);

#endif /* __ACTION_H__*/
