#ifndef __MYREDIS_H__
#define __MYREDIS_H__


enum myredis_status {
  mr__na,  // no record
  mr__need_sync,  // need Redis -> Mysql
  mr__need_sync_back,  // need Mysql -> Redis
  mr__ok,  // record is ok
} ;

enum myredis_op_type {
  ot_cache,
  ot_mq,
  ot_var
};

#define RDS_NAME_SZ  32

struct myredis_s {
  void *ctx ;
  char cache[RDS_NAME_SZ];
  char mq[RDS_NAME_SZ];  // requests pay_svr -> syncd
  char push_msg[RDS_NAME_SZ]; // push messages syncd -> pay_svr
  char var[RDS_NAME_SZ];
  char host[RDS_NAME_SZ];
  int  port ;
} ;
typedef struct myredis_s* myredis_t ;


extern void myredis_release(myredis_t mr);

extern int myredis_init(myredis_t mr, const char *host, int port, char *name);

extern void myredis_dup(myredis_t src, myredis_t dst, char *newname) ;

extern int myredis_read(myredis_t mr, const char *table, const char *key, 
                        dbuffer_t *value);

extern int myredis_write(myredis_t mr, const char *table, char *key, char *value, 
                         int st);

extern bool is_myredis_ok(myredis_t mr);

extern int myredis_add_and_fetch(myredis_t mr, long long *val);

extern int myredis_subscribe_msg(myredis_t mr, dbuffer_t *res);

extern int myredis_publish_msg(myredis_t mr, const char *v);

extern int myredis_reset(myredis_t mr, int type) ;

#endif /* __MYREDIS_H__*/
