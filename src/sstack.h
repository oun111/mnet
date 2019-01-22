#ifndef __SSTACK_H__
#define __SSTACK_H__

#include "kernel.h"
#include "mm_porting.h"


struct sstack_value_s {
  void *v ;
} ;
typedef struct sstack_value_s* sstack_val_t ;

struct sstack_t {
  void *stk ;
  ssize_t top ;
  ssize_t count;
} ;


static inline 
void sstack_init(struct sstack_t *stk)
{
  stk->top   = -1;
  stk->count = 4 ;
  stk->stk   = kmalloc(sizeof(struct sstack_value_s)*stk->count,0L);
}

static inline
void sstack_push(struct sstack_t *stk, void *val)
{
  sstack_val_t ps = 0;


  if (++stk->top>=stk->count) {
    stk->count <<= 1;
    stk->stk = krealloc(stk->stk,sizeof(struct sstack_value_s)*stk->count,0L);
  }

  ps = stk->stk ;
  ps[stk->top].v = val ;
}

static inline 
int sstack_empty(struct sstack_t *stk)
{
  return stk->top==-1;
}

static inline
void* sstack_top(struct sstack_t *stk)
{
  sstack_val_t ps = stk->stk;

  if (sstack_empty(stk))
    return NULL;

  return ps[stk->top].v ;
}

static inline
void sstack_pop(struct sstack_t *stk)
{
  if (!sstack_empty(stk)) 
    stk->top-- ;
}

static inline
void sstack_release(struct sstack_t *stk)
{
  kfree(stk->stk);
}

extern void test_sstack() ;

#endif /* __SSTACK_H__*/

