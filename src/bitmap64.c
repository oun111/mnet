#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "bitmap64.h"
#include "mm_porting.h"


int bm64_init(bitmap64_t bm, size_t nbits)
{
  BITMAP64_INIT(bm,nbits,false);

  printf("total %zu bytes, %zu bits\n",bm->sz,bm->bits);

  return 0;
}

void bm64_release(bitmap64_t bm)
{
  if (bm->map && bm->sz>0L) 
    kfree(bm->map);
}

void bm64_set_bit(bitmap64_t bm, int nbit)
{
  if (nbit>=0 && nbit<bm->bits) 
    bm->map[nbit>>6] |= (1ULL<<(nbit&(63))) ;
}

bool bm64_test_bit(bitmap64_t bm, int nbit)
{
  if (nbit<0 || nbit>=bm->bits)
    return false ;

  return (bm->map[nbit>>6] & (1ULL<<(nbit&(63)))) != 0 ;
}

bool bm64_test_block(bitmap64_t bm, int nbit)
{
  if (nbit<0 || nbit>=bm->bits)
    return false ;

  return (bm->map[nbit>>6] & ~0ULL) != 0;
}

#if TEST_CASES==1
void test_bm64()
{
  int b=0;
  struct bitmap64_s bm ;


  BITMAP64_INIT(&bm,1301,true);

  bm64_set_bit(&bm,31);
  bm64_set_bit(&bm,32);
  bm64_set_bit(&bm,33);
  bm64_set_bit(&bm,66);
  bm64_set_bit(&bm,64);
  bm64_set_bit(&bm,65);
  bm64_set_bit(&bm,63);
  bm64_set_bit(&bm,127);
  bm64_set_bit(&bm,128);
  bm64_set_bit(&bm,129);
  bm64_set_bit(&bm,1208);
  bm64_set_bit(&bm,254);
  bm64_set_bit(&bm,255);
  bm64_set_bit(&bm,256);
  bm64_set_bit(&bm,18);
  bm64_set_bit(&bm,8000);
  bm64_set_bit(&bm,1300);
  bm64_set_bit(&bm,1307);

  
  BITMAP64_FOR_EACH_BITS(b,&bm) {
    if (!bm64_test_block(&bm,b)) {
      b += 63;
      continue ;
    }

    if (bm64_test_bit(&bm,b)) {
      printf("bit %d\n",b);
    }
  }

  bm64_release(&bm);
}
#endif

