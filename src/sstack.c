#include <stddef.h>
#include <stdio.h>
#include "sstack.h"

#if TEST_CASES==1
void test_sstack()
{
  typedef struct tk_s {
    int val;
  } tk_t ;

  tk_t ts[] = {
    {.val = 100,},
    {.val = 1012,},
    {.val = 1023,},
    {.val = 1034,},
    {.val = 1045,},
    {.val = 1056,},
    {.val = 1067,},
    {.val = 1078,},
    {.val = 1089,},
  } ;

  struct sstack_t top;

  sstack_init(&top);

  for (int i=0;i<ARRAY_SIZE(ts);i++) {
    sstack_push(&top,&ts[i]);
    printf("push %d in\n",ts[i].val);
  }

  while (!sstack_empty(&top)) {
    tk_t *pt = (tk_t*)sstack_top(&top);

    printf("pop %d out\n",pt->val);
    sstack_pop(&top);
  }

  sstack_release(&top);

}
#endif

