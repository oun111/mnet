#ifndef __BITMAP64_H__
#define __BITMAP64_H__


struct bitmap64_s {
  unsigned long long *map;
  size_t sz ;
  size_t bits ;
} ;
typedef struct bitmap64_s* bitmap64_t ;


extern int bm64_init(bitmap64_t bm, size_t nbits);

extern void bm64_release(bitmap64_t bm);

extern void bm64_set_bit(bitmap64_t bm, int nbit);

extern bool bm64_test_bit(bitmap64_t bm, int nbit);

extern bool bm64_test_block(bitmap64_t bm, int nbit);


#define BITMAP64_FOR_EACH_BITS(b,bm) \
  for ((b)=0;(b)<(bm)->bits;(b)++)

#define BITMAP64_INIT(__bm,__nbits,__local)   \
  const size_t sz = ((__nbits)>>3) + 1 ;    \
  const size_t aligned = (sz+7) & (~7); \
  if ((__local)==false) (__bm)->map = kmalloc(aligned,0L);    \
  else  (__bm)->map = alloca(aligned);    \
  (__bm)->sz  = aligned ;    \
  (__bm)->bits= aligned << 3;    \
  bzero((__bm)->map,(__bm)->sz)


#endif /* __BITMAP64_H__*/
