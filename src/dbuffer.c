#include <stddef.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "dbuffer.h"
#include "mm_porting.h"
#include "log.h"


struct __attribute__ ((packed)) dbuf_hdr_s  {
  size_t wp ;
  size_t rp ;
  size_t cap ;
  uintptr_t hdr_addr ;
  char data[];
}  ;

typedef struct dbuf_hdr_s* dbuf_hdr_t ;

#define DBUF_HDR_SIZE  sizeof(struct dbuf_hdr_s)

#define DBUF_HDR(b) (b!=NULL?(dbuf_hdr_t)((char*)b-DBUF_HDR_SIZE):NULL)


dbuffer_t alloc_dbuffer(size_t sz)
{
  dbuf_hdr_t h  = kmalloc(sz+DBUF_HDR_SIZE,0L);


  h->wp   = h->rp = 0;
  h->cap  = sz ;
  h->hdr_addr = (uintptr_t)h ;

  return h->data;
}

dbuffer_t alloc_default_dbuffer()
{
  return alloc_dbuffer(0) ;
}

dbuffer_t realloc_dbuffer(dbuffer_t b, size_t sz)
{
  dbuf_hdr_t h = DBUF_HDR(b), newh = 0;
  size_t newsz = sz + 10 ;

  if (!is_dbuffer_valid(b)) {
    return NULL ;
  }

  if (h->cap >= sz) {
    h->wp = h->rp = 0;
    return b ;
  }

  newh = krealloc(h,newsz+DBUF_HDR_SIZE,0L);
  newh->wp = newh->rp = 0;
  newh->cap = newsz ;
  newh->hdr_addr = (uintptr_t)newh ;

  return newh->data;
}

dbuffer_t rearrange_dbuffer(dbuffer_t b, size_t sz)
{
  dbuf_hdr_t h = DBUF_HDR(b), newh= 0 ;
  size_t datalen = 0L ;

  if (!is_dbuffer_valid(b)) {
    return NULL ;
  }

  datalen = dbuffer_data_size(b) ;

  /* free space after write ptr is enough */
  if ((h->cap-h->wp) >= sz) {
    return b;
  }

  /* free spaces is enough, move data to begining */
  if ((h->cap - h->wp + h->rp) >= sz) {
    memmove(b,b+h->rp,datalen);
    h->rp = 0;
    h->wp = datalen ;
    return b ;
  }

  /* not enough spaces, alloc new block */
  size_t newlen = (sz+datalen) + 10  ;

  /* 
   * from man:
   *   The  realloc() function changes the size of the memory block pointed to by 
   *   ptr to size bytes.  The contents will be unchanged in the range from the 
   *   start of the region up to the minimum of the old and new sizes.  If the new 
   *   size is larger than the old size, the added memory will not be initialized.
   */
  newh = krealloc(h,newlen+DBUF_HDR_SIZE,0L);
  if (datalen>0 && newh->rp>0) 
    memcpy(newh->data,newh->data+newh->rp,datalen);

  newh->rp = 0;
  newh->wp = datalen ;
  newh->cap= newlen ;
  newh->hdr_addr = (uintptr_t)newh ;

  return newh->data;
}

int is_dbuffer_valid(dbuffer_t b)
{
  dbuf_hdr_t h = DBUF_HDR(b) ;

  if (!b || !h)
    return 0;

  return h->hdr_addr == (uintptr_t)h ? 1:0 ;
}

int drop_dbuffer(dbuffer_t b)
{
  dbuf_hdr_t h = DBUF_HDR(b) ;

  /* invalid dbuffer block */
  if (!is_dbuffer_valid(b))
    return -1;
  
  kfree(h);
  return 0;
}

size_t dbuffer_data_size(dbuffer_t b)
{
  dbuf_hdr_t h = DBUF_HDR(b) ;

  /* invalid dbuffer block */
  if (!is_dbuffer_valid(b))
    return 0L;

  return h->wp - h->rp ;
}

size_t dbuffer_size(dbuffer_t b)
{
  return is_dbuffer_valid(b)?DBUF_HDR(b)->cap:0L ;
}

dbuffer_t write_dbuffer(dbuffer_t b, void *in, size_t inlen)
{
  dbuf_hdr_t h = DBUF_HDR(b) ;

  if (!is_dbuffer_valid(b)) {
    return NULL ;
  }

  if (h->cap < inlen) {
    size_t newlen = inlen + 10 ;
    dbuf_hdr_t newh = krealloc(h,newlen+DBUF_HDR_SIZE,0L);

    newh->hdr_addr = (uintptr_t)newh ;
    newh->cap = newlen ;
    b = newh->data;
    h = newh;
  }

  memcpy(b,in,inlen);
  h->rp = 0;
  h->wp = inlen ;

  return b;
}

dbuffer_t append_dbuffer(dbuffer_t b, void *in, size_t inlen)
{
  dbuf_hdr_t h = NULL;

  b = rearrange_dbuffer(b,inlen);

  if (!b) 
    return NULL ;

  h = DBUF_HDR(b);

  memcpy(b+h->wp,in,inlen);
  h->wp += inlen ;

  return b;
}

dbuffer_t write_dbuffer_string(dbuffer_t b, char *in, size_t inlen)
{
  b = write_dbuffer(b,in,inlen);

  if (!b)
    return NULL ;

  b[inlen] = '\0';
  //b = append_dbuffer(b,"\0",1);

  return b;
}

dbuffer_t append_dbuffer_string(dbuffer_t b, char *in, size_t inlen)
{
  size_t total = 0L ;


  b = append_dbuffer(b,in,inlen);

  if (!b)
    return NULL;

  total = dbuffer_data_size(b);
  b[total] = '\0';
  //b = append_dbuffer(b,"\0",1);

  return b;
}

dbuffer_t dbuffer_ptr(dbuffer_t b, int rw)
{
  dbuf_hdr_t h = DBUF_HDR(b) ;

  if (!is_dbuffer_valid(b)) 
    return NULL ;

  return rw==0 ? (b+h->rp) : (b+h->wp);
}

int read_dbuffer(dbuffer_t b, char *out, size_t *outlen)
{
  dbuf_hdr_t h = DBUF_HDR(b);
  size_t actual_size = 0L;

  if (!is_dbuffer_valid(b)) {
    return -1;
  }

  actual_size = dbuffer_data_size(b)>*outlen?
                *outlen:dbuffer_data_size(b) ;

  memcpy(out,b+h->rp,actual_size);
  *outlen = actual_size ;
  h->rp  += actual_size ;

  return 0;
}

off_t dbuffer_lseek(dbuffer_t b, off_t offset, int whence, int rw)
{
  dbuf_hdr_t h = DBUF_HDR(b);
  size_t *ptr = NULL;

  if (!is_dbuffer_valid(b)) {
    return -1;
  }

  /* 0 read, 1 write */
  ptr = rw==0? &h->rp : &h->wp ;

  if (whence==SEEK_SET) {
    *ptr = offset ;
  } 
  else if (whence==SEEK_CUR) {
    *ptr += offset ;
  }
  else if (whence==SEEK_END) {
    *ptr = *ptr+offset ;
  }

  return *ptr;
}

#if TEST_CASES==1
int dbuffer_test_case()
{
#define outlen 10240 
  char out[outlen] = "", *po=NULL;
  size_t sz = outlen;
  dbuffer_t buf = alloc_dbuffer(512);
  char *msg = "广州拆城墙之前，大北门的位置，就在今天盘福路与解放北路交界处。城墙从越秀山下来，沿着盘福路一直到今市第一人民医院的东门，然后向西转去，今天市一医院的东西中心大道，就是古城墙的遗址。城墙到人民路再向南转去，人民路也是古城墙的遗址。";

  buf = append_dbuffer(buf,msg,strlen(msg));
  if (!buf) {
    log_error("write dbuffer failed\n");
    return -1;
  }

  read_dbuffer(buf,out,&sz);
  out[sz] = '\0';
  po = out ;
  log_debug("msg: %s, len: %zu, cap: %zu, data sz: %zu, read ptr: %zu, wp: %zu\n",
      po,sz,dbuffer_size(buf),dbuffer_data_size(buf),DBUF_HDR(buf)->rp,DBUF_HDR(buf)->wp);

  char *msg2 = "古代广州有三个著名的湖：菊湖、仙湖和兰湖。前二者都是人工湖，只有兰湖是一个天然大湖。明代以前，城北西的象岗山下，是一泓浩瀚的大水，名为兰湖，又名芝兰湖，北起桂花岗，南达第一津，一碧万顷，恍如青天在水，水在青天。象岗山、席帽山（在今广州医学院一带）起伏于碧波之间。山上松柏茂密，茅芦随风而舞。白云山的蒲涧水经甘溪流入菊湖，菊湖水经越溪流入兰湖，兰湖水经司马涌流入珠江，形成一个流动不息的水网。";

  buf = append_dbuffer(buf,msg2,strlen(msg2));
  if (!buf) {
    log_error("append dbuffer failed\n");
    return -1;
  }

  sz = outlen ;
  read_dbuffer(buf,out,&sz);
  out[sz] = '\0';
  po = out ;
  log_debug("msg2: %s, len: %zu, cap: %zu, data sz: %zu, read ptr: %zu, wp: %zu\n",
      po,sz,dbuffer_size(buf),dbuffer_data_size(buf),DBUF_HDR(buf)->rp,DBUF_HDR(buf)->wp);

  dbuffer_lseek(buf,0,SEEK_SET,0);

  char *msg3 = "一条宽阔的河涌从兰湖向南，经今医国街、盘福路到东风西路一带。东边是高耸的城墙，在盘福路与东风西路交界处是北水关，即城里六脉渠的一个出城口。河涌到了东风西路，即以接近90度角转向西面，出北水关，然后再以近乎90度角转向南面，在第一津接通西濠，直下西堤。后来，兰湖渐渐淤积消失。如今在盘福路医国街内，还能找到兰湖里、兰湖前、兰湖一至五巷地等地名，直到民国时，兰湖里西侧还是一片沼泽地，便是兰湖的遗迹。隋、唐、宋三代，南海县署都是设在这兰湖里一带。隋开皇十年（590年）开始设南海县。唐代时，南海县是广州和岭南东道的治所；宋代开宝五年（972年），合常康、咸宁、番禺、四会共四县为南海县。众所周知，广州实际上分番禺、南海两县各占一半，番禺县署就在中山四路原广州图书馆处，而南海县署曾经长期在盘福路兰湖里。直到元代，南海县署才迁入城中，到明代时迁到今中山六路旧南海县街。当年在兰湖里附近有两座青石桥，一座在盘福路西侧，医国街与朱紫里相交之处，又名青龙桥，建于清初。街巷名字也因桥而名为青石横巷。石桥曾经有个名字叫“三眼桥”，可知桥有三孔，算得上一座比较大型的桥了。后来这一带的河涌逐渐淤塞，填为陆地，桥梁再无作用，遂被拆去。另一座青石桥在青石桥街中段附近，大北门外，左通西濠，右通流花桥。清代后期，水道填为陆地，桥亦荡然无存，只留下一个地名。";

  buf = append_dbuffer(buf,msg3,strlen(msg3));
  if (!buf) {
    log_error("append dbuffer failed\n");
    return -1;
  }

  sz = outlen ;
  read_dbuffer(buf,out,&sz);
  out[sz] = '\0';
  po = out ;
  log_debug("msg3: %s, len: %zu, cap: %zu, data sz: %zu, read ptr: %zu, wp: %zu\n",
      po,sz,dbuffer_size(buf),dbuffer_data_size(buf),DBUF_HDR(buf)->rp,DBUF_HDR(buf)->wp);

  char msg4[] = "衙门在此，肃静回避" ;

  buf = append_dbuffer(buf,msg4,strlen(msg4));
  if (!buf) {
    log_error("append dbuffer failed\n");
    return -1;
  }

  sz = outlen ;
  read_dbuffer(buf,out,&sz);
  out[sz] = '\0';
  po = out ;
  log_debug("msg4: %s, len: %zu, cap: %zu, data sz: %zu, read ptr: %zu, wp: %zu\n",
      po,sz,dbuffer_size(buf),dbuffer_data_size(buf),DBUF_HDR(buf)->rp,DBUF_HDR(buf)->wp);


  dbuffer_lseek(buf,0,SEEK_SET,0);

  sz = outlen ;
  read_dbuffer(buf,out,&sz);
  out[sz] = '\0';
  po = out ;
  log_debug("msg lseek: %s, len: %zu, cap: %zu, data sz: %zu, read ptr: %zu, wp: %zu\n",
      po,sz,dbuffer_size(buf),dbuffer_data_size(buf),DBUF_HDR(buf)->rp,DBUF_HDR(buf)->wp);

  dbuffer_lseek(buf,0,SEEK_SET,0);

  char msg5[] = "当年紧挨着兰湖的街巷，还有一条双井街，在盘福路的西北侧，若干年前我去双井故地寻访时，还见有双井后街、象岗新街等街巷，不知如今尚在否？明代时，这一带属越秀山西麓，汉晋时代是古兰湖的东岸，随着沙泥淤积，到元代就变成陆地了，开始有人在这里居住，居民凿了一口井，下有双孔，因而称为“双眼井”，后来这条街就叫双井街，现在是双井社区。双井如今是没有了。在元代时，这口井一度堙塞，后来又被人疏浚重修，重新冒出清冽的水。遇上天灾人祸，井里还会传出像哭泣的声音。明代成化五年（1469）进士张翊（就是住在诗书路那位大文人）《南海杂咏》有《双眼井》诗，称“双眼井在北城外双井街施水庵侧”。诗是这么写的：“源源复源源，金鳖张两目。记得兵火时，夜半如人哭。” 中国不少古井都有灵异的记录，不是灾前预兆，就是灾后哭泣。";

  buf = append_dbuffer(buf,msg5,strlen(msg5));
  if (!buf) {
    log_error("append dbuffer failed\n");
    return -1;
  }

  sz = outlen ;
  read_dbuffer(buf,out,&sz);
  out[sz] = '\0';
  po = out ;
  log_debug("msg5: %s, len: %zu, cap: %zu, data sz: %zu, read ptr: %zu, wp: %zu\n",
      po,sz,dbuffer_size(buf),dbuffer_data_size(buf),DBUF_HDR(buf)->rp,DBUF_HDR(buf)->wp);

  char msg6[] = "在明代的时候，双眼井旁边有一座施水庵。因为在双眼井旁，又名双井庵。施水庵里面也有一口井，比双眼井还大，喝上去水味相同。到了清代末年，双眼井和施水庵井都没有了。在盘福路的西侧，今天南越王墓那儿，以前叫席帽山，也称象岗。按明、清两代的地方志记载，这座山高约70米，山顶平坦，可容车马，草木郁葱，木多松柏，草多茅芦。南汉皇帝在山上建了一座郊坛。明成化年间（1465－1487），提督两广军务的副都御史韩雍在席帽山上面建墩台，书上说“周长六里”，几乎囊括了整个席帽山山顶，像一座耸立山上的大碉堡，用来保卫广州，现在和兰湖一样，消失无存了。 ";

  buf = append_dbuffer(buf,msg6,strlen(msg6));
  if (!buf) {
    log_error("append dbuffer failed\n");
    return -1;
  }

  sz = outlen ;
  read_dbuffer(buf,out,&sz);
  out[sz] = '\0';
  po = out ;
  log_debug("msg6: %s, len: %zu, cap: %zu, data sz: %zu, read ptr: %zu, wp: %zu\n",
      po,sz,dbuffer_size(buf),dbuffer_data_size(buf),DBUF_HDR(buf)->rp,DBUF_HDR(buf)->wp);

  char msg7[] = "兰湖，广州的避风塘" ;

  buf = write_dbuffer(buf,msg7,strlen(msg7));
  if (!buf) {
    log_error("append dbuffer failed\n");
    return -1;
  }

  sz = outlen ;
  read_dbuffer(buf,out,&sz);
  out[sz] = '\0';
  po = out ;
  log_debug("msg7: %s, len: %zu, cap: %zu, data sz: %zu, read ptr: %zu, wp: %zu\n",
      po,sz,dbuffer_size(buf),dbuffer_data_size(buf),DBUF_HDR(buf)->rp,DBUF_HDR(buf)->wp);

  if (drop_dbuffer(buf)) {
    log_error("failed drop dbuffer\n");
  }

  return 0;
}
#endif

