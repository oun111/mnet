#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include "tree_map.h"
#include "log.h"
#include "kernel.h"
#include "mm_porting.h"
#include "myrbtree.h"
#include "sstack.h"


static int compare(const char *s0, const char *s1)
{
  return strcmp(s0,s1);
}

static
tm_item_t get_tree_map(tree_map_t entry, const char *k)
{
  tm_item_t p = 0;


  if (!MY_RB_TREE_FIND(&entry->u.root,k,p,key,node,compare)) 
    return p ;

  return NULL ;
}


dbuffer_t get_tree_map_value(tree_map_t entry, char *k, size_t kLen)
{
  tm_item_t p = get_tree_map(entry,k);

  return p?p->val:NULL;
}

tree_map_t get_tree_map_nest(tree_map_t entry, char *k, size_t kLen)
{
  tm_item_t p = get_tree_map(entry,k);

  return p?p->nest_map:NULL ;
}

tree_map_t new_tree_map()
{
  tree_map_t entry = kmalloc(sizeof(struct tree_map_entry_s),0L);


  entry->u.root = RB_ROOT ;
  entry->item_count = 0;

  return entry;
}

static int 
__put_tree_map(tree_map_t entry, char *k, size_t kLen, 
             char *v, size_t vLen, bool bNest)
{
  tm_item_t p = get_tree_map(entry,k);


  if (!p) {
    p = kmalloc(sizeof(struct tree_map_item_s),0L);
    if (!p) {
      log_error("allocate new tree map item fail\n");
      return -1 ;
    }

    p->key = alloc_default_dbuffer();
    p->val = alloc_default_dbuffer();

    p->key= write_dbuffer(p->key,k,kLen);
    p->key[kLen] = '\0';

    p->nest_map = bNest?(void*)v:NULL ;

    if (MY_RB_TREE_INSERT(&entry->u.root,p,key,node,compare)) {
      log_error("insert tree map item fail\n");
      kfree(p);
      return -1;
    }

    entry->item_count++ ;
  }

  p->val= write_dbuffer(p->val,v,vLen);
  p->val[vLen] = '\0';

  return 0;
}

int put_tree_map(tree_map_t entry, char *k, size_t kLen, 
                 char *v, size_t vLen)
{
  return __put_tree_map(entry,k,kLen,v,vLen,false);
}

int put_tree_map_nest(tree_map_t entry, char *k, size_t kLen, 
                      void *nest_map)
{
  return __put_tree_map(entry,k,kLen,nest_map,0L,true);
}

static
int drop_tree_map_item_internal(tree_map_t entry, tm_item_t p)
{
  rb_erase(&p->node,&entry->u.root);

  drop_dbuffer(p->key);
  drop_dbuffer(p->val);

  kfree(p);

  if (entry->item_count>0)
    entry->item_count-- ;

  return 0;
}

int drop_tree_map_item(tree_map_t entry, char *k, size_t kLen)
{
  tm_item_t p = get_tree_map(entry,k);


  if (!p) {
    return -1;
  }

  drop_tree_map_item_internal(entry,p);

  return 0;
}

static
int release_all_tree_map_items(tree_map_t entry)
{
  tm_item_t pos,n;

  rbtree_postorder_for_each_entry_safe(pos,n,&entry->u.root,node) {
    drop_tree_map_item_internal(entry,pos);
  }

  return 0;
}

void delete_tree_map(tree_map_t entry)
{
  release_all_tree_map_items(entry);

  kfree(entry);
}

int get_tree_map_item_count(tree_map_t entry)
{
  return entry->item_count ;
}

void dump_tree_map(tree_map_t entry)
{
  struct sstack_t stk_r, stk_p ;
  struct rb_root *proot = &entry->u.root ;
  tm_item_t pos,n;
  bool b1stItem = true ;


  sstack_init(&stk_r);
  sstack_init(&stk_p);

  while(1) {

    if (b1stItem == true) {
      pos = MY_RBTREE_SORTORDER_FIRST_ENTRY(proot,struct tree_map_item_s,node);
    }

    b1stItem = false ;
    MY_RBTREE_SORTORDER_REMAINING_ENTRIES(pos,n,proot,node) {
      if (pos->nest_map) {

        printf("[nest map]: %s -----\n",pos->key);

        sstack_push(&stk_r,proot);
        proot = pos->nest_map ;

        pos = MY_RBTREE_SORTORDER_NEXT_NETRY(pos,node);
        sstack_push(&stk_p,pos);

        b1stItem = true ;
        break ;
      }

      printf("%s : %s\n",pos->key,pos->val);
    }

    if (!b1stItem) {
      proot = sstack_top(&stk_r);
      if (!proot)
        break ;

      pos = sstack_top(&stk_p);

      sstack_pop(&stk_r);
      sstack_pop(&stk_p);
    }

  } // while(1)

  sstack_release(&stk_r);
  sstack_release(&stk_p);
}

#if TEST_CASES==1
void test_tree_map()
{
  tree_map_t entry = new_tree_map();


  printf("tree map testing...\n");

  // test put
  char *k = "base1";
  char *v = "val1";
  put_tree_map(entry,k,strlen(k),v,strlen(v));
  k = "mch_id";
  v = "ll002";
  put_tree_map(entry,k,strlen(k),v,strlen(v));
  k = "arch";
  v = "x86";
  put_tree_map(entry,k,strlen(k),v,strlen(v));
  k = "99level";
  v = "up";
  put_tree_map(entry,k,strlen(k),v,strlen(v));
  k = "weight";
  v = "130g";
  put_tree_map(entry,k,strlen(k),v,strlen(v));
  k = "version";
  v = "v001";
  put_tree_map(entry,k,strlen(k),v,strlen(v));
  k = "req1";
  v = "ual2";
  put_tree_map(entry,k,strlen(k),v,strlen(v));

  // test get
  k = "req1";
  printf("get: key %s -> %s\n",k,get_tree_map_value(entry,k,strlen(k)));
  k = "version";
  printf("get: key %s -> %s\n",k,get_tree_map_value(entry,k,strlen(k)));
  k = "reqp1";
  printf("get: key %s -> %s\n",k,get_tree_map_value(entry,k,strlen(k)));

  // test iteration
  tm_item_t pos,n;

  printf("\nsort-order iterates...\n");
  dump_tree_map(entry);

  printf("\npre-order iterates...\n");
  MY_RBTREE_PREORDER_FOR_EACH_ENTRY_SAFE(pos,n,&entry->u.root,node) {
    printf("%s : %s\n",pos->key,pos->val);
  }

  printf("\nnew-order iterates...\n");
  {
    tree_map_t map2 = new_tree_map();
    tree_map_t map3 = new_tree_map();


    k = "mp2-1";
    v = "v001";
    put_tree_map(map2,k,strlen(k),v,strlen(v));
    k = "mp2-2";
    v = "ual2";
    put_tree_map(map2,k,strlen(k),v,strlen(v));
    k = "map2";
    put_tree_map_nest(entry,k,strlen(k),map2);

    k = "test_mpp3-1";
    v = "v011";
    put_tree_map(map3,k,strlen(k),v,strlen(v));
    k = "test_mpp3-2";
    v = "ua12";
    put_tree_map(map3,k,strlen(k),v,strlen(v));
    k = "test-map3";
    put_tree_map_nest(entry,k,strlen(k),map3);

    dump_tree_map(entry);

    delete_tree_map(map2);
    delete_tree_map(map3);

  }

  delete_tree_map(entry);

  printf("done!\n");

  exit(0);
}
#endif

