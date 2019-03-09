
#ifndef __JSONS_H__
#define __JSONS_H__

#include "sstack.h"
#include "list.h"
#include "tree_map.h"

#define MAX_KEY_LEN  64
//#define MAX_VAL_LEN  256

typedef struct jsonKV_s {
  /* 0: key-value, 1: key-object, 2: key only */
  unsigned char type ;

  /* related to parent's children list */
  struct list_head upper ;

  /* children list of current node */
  struct list_head children ;
  size_t num_children ;

#if 0
  char key[MAX_KEY_LEN];

  char value[MAX_VAL_LEN];
#else
  dbuffer_t key ;

  dbuffer_t value ;
#endif

  struct jsonKV_s *parent ;

} jsonKV_t;


extern jsonKV_t* jsons_parse(char *s);

extern int jsons_release(jsonKV_t *root);

extern jsonKV_t* jsons_find(jsonKV_t *root, const char *key);

extern void jsons_dump(jsonKV_t *root);

extern void test_jsons();

extern char* jsons_string(char *in, size_t *inlen) ;

extern int jsons_integer(char *in);

extern jsonKV_t* jsons_parse_tree_map(tree_map_t entry);

extern int jsons_toString(jsonKV_t *root, dbuffer_t *outb, bool add_del);

extern int treemap_to_jsons_str(tree_map_t in, dbuffer_t *outb);

extern tree_map_t jsons_to_treemap(jsonKV_t *root);

extern int jstr_add_delimiter(dbuffer_t jstr, dbuffer_t *outb);

extern jsonKV_t* jsons_add_to_array(jsonKV_t *root, jsonKV_t *child);

#endif /* __JSONS_H__*/

