#ifndef __MYREDIS_H__
#define __MYREDIS_H__


enum myredis_status {
  mr__na,  // no record
  mr__need_sync,  // need Redis -> Mysql
  mr__need_sync_back,  // need Mysql -> Redis
  mr__ok,  // record is ok
} ;

struct myredis_s {
  void *ctx ;
  char cache_name[32];
  char mq_name[32];
} ;
typedef struct myredis_s* myredis_t ;


extern void myredis_release(myredis_t mr);

extern int myredis_init(myredis_t mr, const char *host, int port, char *name);


#endif /* __MYREDIS_H__*/
