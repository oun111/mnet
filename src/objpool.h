#ifndef __OBJPOOL_H__
#define __OBJPOOL_H__

#include <unistd.h>
#include "list.h"
#include "mm_porting.h"

struct objPool_s {
  struct kmem_cache *cache ;
  struct list_head free_pool ;
  size_t pool_size;
} ;

typedef struct objPool_s* objPool_t ;


#define MAX_OBJ_POOL  100000

#define create_obj_pool(desc,ps,t)  ({\
  objPool_t p = kmalloc(sizeof(struct objPool_s),0L); \
  if (p) {  \
    INIT_LIST_HEAD(&p->free_pool);  \
    p->cache = kmem_cache_create(desc,sizeof(t),0L,0L,NULL);  \
    p->pool_size = (ps)<MAX_OBJ_POOL?(ps):MAX_OBJ_POOL ;  \
    for (int i=0;i<pool_size;i++) {   \
      typeof(t) *po = kmem_cache_alloc(p->cache,0L);  \
      list_add(&po->pool_item,&p->free_pool);  \
    }  \
  }  \
  p;  \
})

#define release_obj_pool(op,t)  ({\
  typeof(t) *pos,*n; \
  list_for_each_entry_safe (pos,n,&(op)->free_pool,pool_item) {  \
    list_del(&pos->pool_item);  \
    kmem_cache_free((op)->cache,pos);  \
  }  \
  kmem_cache_destroy((op)->cache);   \
  kfree(op);  \
})

#define obj_pool_alloc(op,t)  ({ \
  typeof(t) *pos = list_first_entry_or_null(&(op)->free_pool,t,pool_item); \
  if (pos) list_del(&pos->pool_item);  \
  pos; \
})

#define obj_pool_free(op,item)  ({\
  list_add(&(item)->pool_item,&(op)->free_pool);  \
})

#define list_for_each_objPool_item(pos,n,e) \
  list_for_each_entry_safe(pos,n,&(e)->free_pool,pool_item) 

#endif /* __OBJPOOL_H__*/
