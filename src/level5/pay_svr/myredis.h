#ifndef __MYREDIS_H__
#define __MYREDIS_H__


enum myredis_status {
  mr__na,  // no record
  mr__need_sync,  // need Redis -> Mysql
  mr__need_sync_back,  // need Mysql -> Redis
  mr__ok,  // record is ok
} ;

#define RDS_CACHE_NAME_SZ  32
#define RDS_MQ_NAME_SZ     32

struct myredis_s {
  void *ctx ;
  char cache_name[RDS_CACHE_NAME_SZ];
  char mq_name[RDS_MQ_NAME_SZ];
} ;
typedef struct myredis_s* myredis_t ;


extern void myredis_release(myredis_t mr);

extern int myredis_init(myredis_t mr, const char *host, int port, char *name);

extern int myredis_read(myredis_t mr, const char *table, const char *key, 
                        dbuffer_t *value);

extern int myredis_write(myredis_t mr, const char *table, char *key, char *value, 
                         int st);

extern bool is_myredis_ok(myredis_t mr);

extern void myredis_change_name(myredis_t mr, char *name);

#endif /* __MYREDIS_H__*/
