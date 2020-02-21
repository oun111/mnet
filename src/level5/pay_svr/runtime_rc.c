#include <stdlib.h>
#include <stdbool.h>
#include "runtime_rc.h"
#include "dbuffer.h"
#include "myredis.h"
#include "log.h"


static
const char* rt_strings[] = 
{
  "na",
  "ORDERS",
  "AMOUNT",
} ;

int runtime_rc_save(runtime_rc_t entry, int type, char *appid, void *value)
{
  char *rts = NULL;
  void *v = NULL;
  size_t szv = 64;


  if (type<=rt_na || type>=rt_max) {
    log_error("unknown rc data type %d\n", type);
    return -1;
  }

  v = alloca(szv);

  if (type==rt_orders) snprintf(v,szv,"%d",*(int*)value);
  else 
  if (type==rt_amount) snprintf(v,szv,"%f",*(double*)value);

  rts = (char*)rt_strings[type];

  // write to redis
  if (myredis_write((myredis_t)entry->handle,appid,rts,v,mr__ok)) {
    return -1;
  }

  return 0;
}

int runtime_rc_fetch(runtime_rc_t entry, int type, char *appid, void *value)
{
  char *rts  = NULL;
  dbuffer_t v= NULL;
  int ret = -1;


  if (type<=rt_na || type>=rt_max) {
    log_error("unknown rc data type %d\n", type);
    return -1;
  }

  v  = alloc_default_dbuffer();
  rts= (char*)rt_strings[type];
  // read from redis
  if (myredis_read((myredis_t)entry->handle,appid,rts,&v)) {
    goto __done ;
  }

  if (type==rt_orders) *(int*)value = atoi(v);
  else
  if (type==rt_amount) *(double*)value = atof(v);

  ret = 0;

__done:
  drop_dbuffer(v);

  return ret;
}

void init_runtime_rc(runtime_rc_t entry, void *handle)
{
  entry->handle = handle ;

  entry->save   = runtime_rc_save ;

  entry->fetch  = runtime_rc_fetch ;
}

