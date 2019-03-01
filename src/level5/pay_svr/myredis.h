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

typedef int(*fmt_conv)(tree_map_t,dbuffer_t*);


extern void myredis_release(myredis_t mr);

extern int myredis_init(myredis_t mr, const char *host, int port, char *name);

extern int myredis_read(myredis_t mr, const char *table, const char *key, fmt_conv, dbuffer_t *value);

extern bool is_myredis_ok(myredis_t mr);

#endif /* __MYREDIS_H__*/
