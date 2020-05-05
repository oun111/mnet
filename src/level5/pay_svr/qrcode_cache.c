#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "dbuffer.h"
#include "qrcode_cache.h"
#include "myredis.h"
#include "log.h"


int qrcode_cache_save(qrcode_cache_t entry, char *orderid, const char *qrcode)
{
  // write to redis
  if (myredis_write((myredis_t)entry->handle,orderid,"qrcode",(char*)qrcode,mr__ok)) {
    return -1;
  }

  return 0;
}

int qrcode_cache_fetch(qrcode_cache_t entry, char *orderid, dbuffer_t *qrbuf)
{
  // read from redis
  if (myredis_read((myredis_t)entry->handle,orderid,"qrcode",qrbuf)) {
    return -1 ;
  }

  return 0;
}

void init_qrcode_cache(qrcode_cache_t entry, void *handle, char *name)
{
  entry->handle = handle ;

  if (!name) name = "qrcc";
  strncpy(entry->name,name,sizeof(entry->name));

  log_info("qrcode cache name: %s\n",entry->name);
}

