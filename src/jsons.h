
#ifndef __JSONS_H__
#define __JSONS_H__

#include "sstack.h"
#include "list.h"
#include "tree_map.h"

#define MAX_KEY_LEN  64
#define MAX_VAL_LEN  256

typedef struct jsonKV_s {
  /* 0: key-value, 1: key-object, 2: key only */
  unsigned char type ;

  /* related to parent's children list */
  struct list_head upper ;

  /* children list of current node */
  struct list_head children ;
  size_t num_children ;

  char key[MAX_KEY_LEN];

  char value[MAX_VAL_LEN];

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

extern int jsons_toString(jsonKV_t *root, dbuffer_t *outb);

#endif /* __JSONS_H__*/

