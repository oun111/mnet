
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <ctype.h>
#include <stdio.h>
#include "jsons.h"
#include "mm_porting.h"
#include "dbuffer.h"
#include "file_cache.h"
#include "myrbtree.h"


/* the 'comment' keyword */
static const char *commKW = "__comment";

/* object types */
enum eObjectType {
  keyValue,
  keyList,
  keyOnly
} ;

enum ePtnType {
  pFind,
  pDump,
  pRelease,
  pToStr,  // json -> string
  pToTreemap, // json -> tree map
};

struct jsonsPtnInfo {
  dbuffer_t str;

  struct sstack_t k;
} ;

#define NEXT_CHILD(s,p) ({      \
  sstack_pop(&(s));                 \
  (p) = (p)->next ;                 \
  sstack_push(&(s),p);            \
})

#define FUNC_FIND(__p,__key,__res) do{  \
  char *__key0 = (__p)->key; \
  char *__key1 = __key; \
  char *pv = 0; \
  size_t vl = 0L ; \
  if (*__key0==*__key1) { \
    if (!strcmp(__key0,__key1)) { \
      (__res) = (__p) ;                \
      break;                          \
    }                                  \
  }        \
  else if (*__key0=='\"' || *__key0=='\'') { \
    pv = jsons_string(__key0,&vl); \
    if (vl==strlen(__key1) && \
        !strncmp(pv,__key1,strlen(__key1))) { \
      (__res) = (__p) ;                \
      break;                          \
    } \
  } \
  else if (*__key1=='\"' || *__key1=='\'') { \
    pv = jsons_string(__key1,&vl);  \
    if (strlen(__key0)==vl && \
        !strncmp(__key0,pv,strlen(__key0))) { \
      (__res) = (__p) ;                \
      break;                          \
    } \
  } \
}while(0)

#define FUNC_DUMP(__p,__level) do{   \
  printf("%d ",__level);           \
  for (int i=0;i<(__level);i++)      \
    printf("%s","-");            \
  printf("%s<%p> ", (__p)->key,(__p)->parent);         \
  if ((__p)->type==keyList)          \
    printf("(%zu)\n",(__p)->num_children); \
  else if ((__p)->type==keyValue)          \
    printf(" => %s \n", (__p)->value); \
}while(0)

#define NOT_LAST_CHILD(__p__,__parent__) \
  (__parent__) && (__parent__)->num_children>1 && (__p__)->upper.next!=&(__parent__)->children

#define FUNC_TOSTR_0(__p__,__arg__) do{ \
  struct jsonsPtnInfo *ti = (struct jsonsPtnInfo*)(void*)(__arg__); \
  if (strcmp((__p__)->key,"root")) { \
    append_dbuf_str(ti->str,(__p__)->key); \
    append_dbuf_str(ti->str,":"); \
  } \
  if ((__p__)->type==keyValue) {  \
    append_dbuf_str(ti->str,(__p__)->value); \
    if (NOT_LAST_CHILD(__p__,(__p__)->parent)) \
      append_dbuf_str(ti->str,","); \
  } \
  else if ((__p__)->type==keyList) \
    append_dbuf_str(ti->str,"{"); \
} while(0)

#define FUNC_TOSTR_1(__p__,__arg__) do{\
  struct jsonsPtnInfo *ti = (struct jsonsPtnInfo*)(void*)(__arg__); \
  append_dbuf_str(ti->str,"}"); \
  if (NOT_LAST_CHILD(__p__,(__p__)->parent)) \
    append_dbuf_str(ti->str,","); \
}while(0)


#define FUNC_TOTREEMAP_0(__p__,__arg__) do{\
  struct jsonsPtnInfo *ti = (struct jsonsPtnInfo*)(void*)(__arg__); \
  tree_map_t prt = sstack_top(&ti->k); \
  size_t vl= 0L;  \
  char *pv = jsons_string(__p__->key,&vl); \
  if ((__p__)->type==keyList) {\
    tree_map_t chld = new_tree_map(); \
    put_tree_map_nest(prt,pv,vl,chld); \
    sstack_push(&ti->k,chld); \
  } \
  else if ((__p__)->type==keyValue) {\
    size_t ln= 0L;  \
    char *val = jsons_string(__p__->value,&ln); \
    put_tree_map(prt,pv,vl,val,ln); \
  } \
}while(0)

#define FUNC_TOTREEMAP_1(__p__,__arg__) do{\
  struct jsonsPtnInfo *ti = (struct jsonsPtnInfo*)(void*)(__arg__); \
  sstack_pop(&ti->k);  \
}while(0)

//static 
void free_node(jsonKV_t *node)
{
  drop_dbuffer(node->key);
  drop_dbuffer(node->value);

  if (node)
    kfree(node);
}

static
jsonKV_t* new_node(char *key)
{
  jsonKV_t *node = kmalloc(sizeof(jsonKV_t),0L) ;

  //strcpy(node->key,key);
  INIT_LIST_HEAD(&node->upper);
  INIT_LIST_HEAD(&node->children);
  node->num_children = 0L;
  node->parent = NULL;
  node->type = keyValue;
  node->key  = alloc_default_dbuffer();
  node->value= alloc_default_dbuffer();

  write_dbuf_str(node->key,key);

  return node ;
}

static inline
jsonKV_t* jsons_pattern(jsonKV_t *root, int function, void *arg)
{
  jsonKV_t *p = root, *res= 0 ;
  struct list_head *pl= NULL ;
  struct sstack_t stk,stk1;
  int level = 0;

  if (!root)
    return NULL;

  sstack_init(&stk);
  sstack_init(&stk1);

  while (1) {

    if (!sstack_empty(&stk) && !sstack_empty(&stk1)) {
      jsonKV_t *p0 = sstack_top(&stk);
      struct list_head *pl0 = sstack_top(&stk1);

      if (pl0==&p0->children) {

        if (function==pToStr) 
          FUNC_TOSTR_1(p0,arg);

        if (function==pToTreemap) 
          FUNC_TOTREEMAP_1(p0,arg);

        sstack_pop(&stk);
        sstack_pop(&stk1);
        level-- ;

        if (function==pRelease) 
          res = p0;

        /* check parent */
        if (!sstack_empty(&stk) && !sstack_empty(&stk1)) {
          p0 = sstack_top(&stk);
          pl0 = sstack_top(&stk1);
          /* move to next child */
          NEXT_CHILD(stk1,pl0);
          p = list_entry(pl0,jsonKV_t,upper);
        }

        /* end scan children list, free the node */
        if (function==pRelease) {
          list_del(&res->upper);
          free_node(res);
        }

        if (sstack_empty(&stk)) 
          break ;

        continue ;
      }
    }

    if (function==pFind) 
      FUNC_FIND(p,arg,res);

    if (function==pDump) 
      FUNC_DUMP(p,level);

    if (function==pToStr) 
      FUNC_TOSTR_0(p,arg);

    if (function==pToTreemap) 
      FUNC_TOTREEMAP_0(p,arg);

    /* the node has children list */
    if (p->type==keyList) {
      sstack_push(&stk,p);
      pl = p->children.next ;
      sstack_push(&stk1,pl);
      /* has children in it, move to the 1st child node */
      p = list_entry(pl,jsonKV_t,upper);
      level++ ;
    } 
    else if (p->type==keyValue) {
      if (function==pRelease) 
        res = p;

      /* it's a key-value node, so move to next brother node */
      if (!sstack_empty(&stk) && !sstack_empty(&stk1)) {
        p  = sstack_top(&stk);
        pl = sstack_top(&stk1);
        NEXT_CHILD(stk1,pl);
        if (pl!=&p->children) 
          p = list_entry(pl,jsonKV_t,upper);
      }
      else
        break;

      /* end process the node, so free it */
      if (function==pRelease) {
        list_del(&res->upper);
        free_node(res);
      }

    }

  } /* end while */

  sstack_release(&stk);
  sstack_release(&stk1);

#if 0
  if (function==pRelease)
    free_node(root);
#endif

  return res ;

}

jsonKV_t* jsons_find(jsonKV_t *root, const char *key)
{
  if (!key)
    return NULL ;

  return jsons_pattern(root,pFind,(void*)key);
}

void jsons_dump(jsonKV_t *root)
{
  jsons_pattern(root,pDump,NULL);
}

int jsons_release(jsonKV_t *root)
{
  jsons_pattern(root,pRelease,NULL);
  return 0;
}

int jsons_toString(jsonKV_t *root, dbuffer_t *outb)
{
  struct jsonsPtnInfo ti ;


  ti.str = alloc_default_dbuffer();

  jsons_pattern(root,pToStr,(void*)&ti);

  // XXX:
  write_dbuf_str(*outb,ti.str);

  drop_dbuffer(ti.str);

  return 0;
}

tree_map_t jsons_to_treemap(jsonKV_t *root)
{
  struct jsonsPtnInfo ti = {NULL};
  tree_map_t base_map = new_tree_map();


  sstack_init(&ti.k);

  sstack_push(&ti.k,base_map);

  jsons_pattern(root,pToTreemap,(void*)&ti);

  sstack_release(&ti.k);

  return base_map;
}

int treemap_to_jsons_str(tree_map_t in, dbuffer_t *outb)
{
  jsonKV_t *pr = jsons_parse_tree_map(in);

  jsons_toString(pr,outb);
  jsons_release(pr);

  return 0;
}

static
int jsons_lex_read(char *s, dbuffer_t *tkn, int pos)
{
  uint16_t p = pos, p0=0;
  //size_t sz = 0;

  /* 
   * eliminate spaces 
   */
  for (;p<strlen(s)&&isspace(s[p]);p++) ;
  if (p>=strlen(s))
    return p;
  /* get a symbol */
  if (s[p]=='{' || s[p]=='}' || s[p]=='[' ||
     s[p]==']' || s[p]==':' || s[p]==',') {
    *tkn = write_dbuffer_string(*tkn,&s[p],1);
    return p+1 ;
  }
  /* 
   * get a normal string token
   */
  if (s[p]=='\"' || s[p]=='\'') {
    /*const*/ char endc = s[p] ;
    for (++p,p0=p;p<strlen(s)&&s[p]!=endc;p++);
    if (p>=strlen(s))
      return p;
    *tkn = write_dbuffer(*tkn,&endc,1);
    *tkn = append_dbuffer(*tkn,s+p0,(p-p0));
    *tkn = append_dbuffer_string(*tkn,&endc,1);
    return p+1 ;
  }
  /* 
   * get a normal integer token
   */
  {
    for (p0=p;p<strlen(s)&&s[p]!=','&&s[p]!=']'
         &&s[p]!='}'&&!isspace(s[p]);p++);
    if (p>=strlen(s))
      return p;
    *tkn = write_dbuffer_string(*tkn,s+p0,(p-p0));
    return p+1 ;
  }

  return -1;
}

static 
int jsons_eliminate_comments(char *s)
{
  int level = 0;
  uint16_t p = 0, sp=0;

  for (;p<(strlen(s)-1);p++) {
    if (s[p]=='/' && s[p+1]=='*' && !level++) {
      sp = p ;
      p += 2;
    } 
    if (s[p]=='*' && s[p+1]=='/' && !--level) {
      p+=2;
      //s.erase(sp,p-sp);
      memset(s+sp,' ',p-sp);
      p = sp ;
    }
  }
  return 0;
}

jsonKV_t* jsons_parse(char *s)
{
  int pos=0 ;
  dbuffer_t tkn = alloc_default_dbuffer() ;
  jsonKV_t *p = 0, *tmp=0, *m_root = 0;
  struct sstack_t stk ;
  bool bEval = false, bKey = false, 
    /* it's comment */
    bComm = false;

  jsons_eliminate_comments(s);
  /* tree root */
  m_root = new_node((char*)"root");
  p = m_root ;

  sstack_init(&stk);
  sstack_push(&stk,p);

  /* traverse and create the analyzing tree */
  while ((pos=jsons_lex_read(s,&tkn,pos))>=0 && pos<(int)strlen(s)) {

    if (*tkn=='{' || *tkn=='[') {
      p = (jsonKV_t*)sstack_top(&stk);
      p->type = keyList ;
      bEval = false ;
    } else if (*tkn=='}') {
      sstack_pop(&stk);
    } else if (*tkn==':') {
      bEval = true ;
      bKey  = false ;
    } else if (*tkn==',' || *tkn==']') {
      if (bKey) { 
        p = (jsonKV_t*)sstack_top(&stk);
        p->type=keyOnly;
        /* pop out the key-only onde */
        sstack_pop(&stk);
        bKey = false; 
        /* end of array? pop it out! */
        if (*tkn==']') 
          sstack_pop(&stk);
      }
    } else if (!strcmp(tkn,commKW)) {
      bComm = true ;
    } else {
      if (!bComm) {
        /* it's the value */
        if (bEval) {
          p = (jsonKV_t*)sstack_top(&stk);
          sstack_pop(&stk);
          write_dbuf_str(p->value,tkn);
          p->type  = keyValue ;
        } else {
          /* it's a key */
          tmp = new_node((char*)tkn);
          p = (jsonKV_t*)sstack_top(&stk);
          list_add(&tmp->upper,&p->children);
          p->type = keyList ;
          p->num_children ++ ;
          tmp->parent = p ;
          sstack_push(&stk,tmp);
          bKey = true;
        }
      }
      bEval = false ;
      bComm = false ;
    } /* end else */
  }

  sstack_release(&stk);

  drop_dbuffer(tkn);

  return m_root;
}

static 
jsonKV_t* attach_parent(jsonKV_t *parent, const char *key, int nodeType)
{
  jsonKV_t *pj = new_node((char*)key);

  pj->parent = parent ;
  pj->type   = nodeType;
  list_add(&pj->upper,&parent->children);
  parent->num_children ++ ;

  return pj ;
}

static
dbuffer_t create_jsons_string(dbuffer_t jstr, const char *s)
{
  write_dbuf_str(jstr,"\"");
  append_dbuf_str(jstr,s);
  append_dbuf_str(jstr,"\"");

  return jstr;
}

jsonKV_t* jsons_parse_tree_map(tree_map_t entry)
{
  jsonKV_t *js_root = new_node((char*)"root"), *pj = 0;
  struct sstack_t stk_r, stk_p, stk_s ;
  struct rb_root *m_root = &entry->u.root ;
  tm_item_t pos,n;
  bool b1stItem = true ;
  dbuffer_t jstr = alloc_default_dbuffer();


  sstack_init(&stk_r);
  sstack_init(&stk_p);
  sstack_init(&stk_s);

  js_root->type = keyList;

  while(1) {

    if (b1stItem == true) {
      pos = MY_RBTREE_SORTORDER_FIRST_ENTRY(m_root,struct tree_map_item_s,node);
    }

    b1stItem = false ;
    MY_RBTREE_SORTORDER_REMAINING_ENTRIES(pos,n,m_root,node) {

      if (pos->nest_map) {

        sstack_push(&stk_s,js_root);
        jstr = create_jsons_string(jstr,pos->key);
        js_root = attach_parent(js_root,jstr,keyList);

        sstack_push(&stk_r,m_root);
        m_root = pos->nest_map ;

        pos = MY_RBTREE_SORTORDER_NEXT_NETRY(pos,node);
        sstack_push(&stk_p,pos);

        b1stItem = true ;
        break ;
      }
      
      jstr = create_jsons_string(jstr,pos->key);
      pj = attach_parent(js_root,jstr,keyValue);

      // integer
      if (pos->val[0]=='$') {
        jstr = write_dbuffer_string(jstr,pos->val+1,strlen(pos->val)-1);
      }
      else {
        jstr = create_jsons_string(jstr,pos->val);
      }
      write_dbuf_str(pj->value,jstr);
    }

    if (!b1stItem) {
      m_root = sstack_top(&stk_r);
      if (!m_root)
        break ;

      pos = sstack_top(&stk_p);

      js_root = sstack_top(&stk_s);

      sstack_pop(&stk_r);
      sstack_pop(&stk_p);
      sstack_pop(&stk_s);
    }

  } // while(1)

  sstack_release(&stk_r);
  sstack_release(&stk_p);
  sstack_release(&stk_s);

  drop_dbuffer(jstr);

  return js_root ;
}

char* jsons_string(char *in, size_t *inlen) 
{
  if (*in=='\'' || *in=='\"') {
    *inlen = strlen(in+1)-1;
    return in+1;
  }

  if (inlen)
    *inlen = strlen(in) ;

  return in ;
}

int jsons_integer(char *in)
{
  return atoi(in);
}

#if TEST_CASES==1
void test_jsons()
{
#if 1
  char inb[] = 
  "\"DataNodes\": { "
  "  \"dn1\": {  "
  "   \"Address\": \"127.0.0.1:3306\", "
  "   \"ListenPort\": 3611, "
  "   \"Schema\": \"orcl\", "
  "   \"ParamA\": \"guess\", "
  "   \"ItemCount\": 31, "
  "   \"Auth\": {\"root\":\"123\", \"nmts\":\"abc123\",}  "
  "  },"
  "  \"dn2\": {  "
  "   \"Schema\": \"bsc\", "
  "   \"ParamB\": \"try\", "
  "  },"
  "}, ";
#else
  dbuffer_t inb  = alloc_dbuffer(0);

  if (load_file("/tmp/mp2_conf.json",&inb)) {
    printf("no such file!\n");
    return ;
  }
#endif

  jsonKV_t *root = jsons_parse(inb);

  /* XXX: test */
  jsons_dump(root);
  printf("*************\n");
  jsons_dump(jsons_find(root,(char*)"DataNodes"));
  printf("*************\n");
  jsons_dump(jsons_find(root,(char*)"\"Schema\""));
  printf("*************\n");
  jsons_dump(jsons_find(root,(char*)"\"db2\""));
  printf("*************\n");
  jsons_dump(jsons_find(root,(char*)"ParamB"));
  printf("*************\n");
  jsons_dump(jsons_find(root,(char*)"ListenPort"));

  // test to string
  {
    dbuffer_t str = alloc_default_dbuffer();
    jsons_toString(root,&str);

    printf("string result >>>>>>  %s\n",str);
    drop_dbuffer(str);
  }

  printf("freeing...\n");
  jsons_release(root);

  // test tree_map -> jsons -> string
  printf("*************\n");
  {
    tree_map_t tmap = new_tree_map();
    tree_map_t map2 = new_tree_map();
    tree_map_t map3 = new_tree_map();
    tree_map_t map4 = new_tree_map();
    char *key=0, *val = 0;

    key = "avv1";
    val = "val1";
    put_tree_map_string(tmap,key,val);
    key = "ba2";
    val = "wza";
    put_tree_map_string(tmap,key,val);
    key = "ca3";
    val = "val2";
    put_tree_map_string(tmap,key,val);
    key = "xa3";
    val = "ccl2";
    put_tree_map_string(tmap,key,val);

    key = "mp2-test1";
    val = "vm11";
    put_tree_map_string(map2,key,val);
    key = "mp2-test2";
    val = "o21";
    put_tree_map_string(map2,key,val);
    key = "map2";
    put_tree_map_nest(tmap,key,strlen(key),map2);

    key = "cmap-test1";
    val = "cm1";
    put_tree_map_string(map3,key,val);
    key = "cmap-test2";
    val = "c21";
    put_tree_map_string(map3,key,val);
    key = "cmap";
    put_tree_map_nest(tmap,key,strlen(key),map3);

    key = "1submap-test0";
    val = "sm1";
    put_tree_map_string(map4,key,val);
    key = "2submap-test";
    val = "sm2";
    put_tree_map_string(map4,key,val);
    key = "submap";
    put_tree_map_nest(map2,key,strlen(key),map4);


    root = jsons_parse_tree_map(tmap);
    printf("*************\n");
    jsons_dump(root);

    printf("*************\n");
    {
      dbuffer_t str = alloc_default_dbuffer();
      jsons_toString(root,&str);

      printf("string result >>>>>>  %s\n",str);
      drop_dbuffer(str);
    }

    printf("*************\n");
    tree_map_t totmap = jsons_to_treemap(root);
    dump_tree_map(totmap);

    delete_tree_map(totmap);

    jsons_release(root);
    delete_tree_map(tmap);
  }

  exit(0);
}
#endif
