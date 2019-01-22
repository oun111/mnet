#ifndef __TREE_MAP_H__
#define __TREE_MAP_H__

#include "rbtree.h"
#include "dbuffer.h"


struct tree_map_item_s {
  //unsigned long id ;

  dbuffer_t key ;
  dbuffer_t val;

  void *nest_map;

  struct rb_node node ;
} ;
typedef struct tree_map_item_s* tm_item_t ;


struct tree_map_entry_s {
  union {
    struct rb_root root;
  } u;

  int item_count ;
} ;
typedef struct tree_map_entry_s* tree_map_t ;



extern tree_map_t new_tree_map();

extern void delete_tree_map(tree_map_t entry);

extern dbuffer_t get_tree_map_value(tree_map_t entry, char *k, size_t kLen);

extern int put_tree_map(tree_map_t entry, char *k, size_t kLen, char *v, size_t vLen);

extern int put_tree_map_nest(tree_map_t entry, char *k, size_t kLen, void *nest_map);

extern int drop_tree_map_item(tree_map_t entry, char *k, size_t kLen);

extern int get_tree_map_item_count(tree_map_t entry);


#endif /* __TREE_MAP_H__*/
