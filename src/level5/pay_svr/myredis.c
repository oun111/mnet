#include <string.h>
#include <stdlib.h>
#include <hiredis/hiredis.h>
#include "myredis.h"
#include "dbuffer.h"
#include "jsons.h"
#include "tree_map.h"

/**
 * Redis - Mysql 数据同步策略：
 *
 * 写入：
 *
 *  1. 应用写数据到redis，置status=need-sync，如有记录则覆盖
 *
 *  2. 同步程序扫描并拆分该记录写入Mysql，置status=ok
 *
 * 读取：
 *
 *  1. 有记录且status=ok/need-sync，直接返回记录
 *
 *  2. 无记录，加一条Redis空记录并置status=need-sync-back，
 *     同步程序扫描并尝试从Mysql读取：
 *
 *      若有，把Mysql记录合并Redis格式覆盖Redis空记录并置status=ok;
 *      若无，更新Redis空记录置status=n-a
 *
 *  3. 应用根据status=need-sync-back/n-a 返回相应内容
 *
 */

#define REDIS_CTX  ((redisContext*)(mr->ctx))


void myredis_release(myredis_t mr)
{
  if (REDIS_CTX) {
    redisFree(REDIS_CTX);
    mr->ctx = NULL;
  }
}

int myredis_init(myredis_t mr, const char *host, int port, char *name)
{
  myredis_release(mr);

  mr->ctx = redisConnect(host,port);

  if (!REDIS_CTX || REDIS_CTX->err!=REDIS_OK) {
    printf("redis connect fail: %s\n",REDIS_CTX->errstr);
    myredis_release(mr);
    return -1;
  }

  strncpy(mr->name,name,sizeof(mr->name));

  return 0;
}

int myredis_write(myredis_t mr, const char *table, char *key, char *value)
{
  size_t kl = strlen(table)+strlen(key)+10;
  size_t vl = kl+strlen(value)+100;
  dbuffer_t k = alloc_dbuffer(kl);
  dbuffer_t v = alloc_dbuffer(vl);
  int ret = 0;
  redisReply *rc = 0;


  snprintf(k,kl,"%s_%s",table,key);
  snprintf(v,vl,"{\"status\" : %d, \"table\" : \"%s\", \"value\" : \"%s\" }",
           mr__need_sync, table, value);


  rc = (redisReply*)redisCommand(REDIS_CTX,"hset %s %s '%s'",mr->name,k,v);
  if (rc->type==REDIS_REPLY_ERROR) {
    printf("write to redis fail: %s\n",rc->str) ;
    ret = -1;
  }

  drop_dbuffer(k);
  drop_dbuffer(v);

  return ret;
}

int myredis_read(myredis_t mr, const char *table, const char *key, dbuffer_t *value)
{
  size_t kl = strlen(table)+strlen(key)+10;
  dbuffer_t k = alloc_dbuffer(kl);
  dbuffer_t v = alloc_default_dbuffer();
  redisReply *rc = 0;
  int ret = 0, status = 0;
  jsonKV_t *pr = 0;
  tree_map_t map = 0;
  dbuffer_t pd = 0;


  snprintf(k,kl,"%s_%s",table,key);

  rc = (redisReply*)redisCommand(REDIS_CTX,"hget %s %s",mr->name,k);
  if (rc->type==REDIS_REPLY_ERROR) {
    printf("read from redis fail: %s\n",rc->str) ;
    ret = -1;
    goto __end ;
  }

  if (rc->type!=REDIS_REPLY_STRING) {
    printf("not redis string, type=%d\n",rc->type);
    ret = -1;
    goto __end ;
  }

  v = write_dbuffer_string(v,rc->str,rc->len);
  printf("ret string: %s\n",v);


  // parse the result
  pr = jsons_parse(v);
  map = jsons_to_treemap(pr);
  pd = get_tree_map_value(map,"status");

  if (!pd) {
    printf("no status found!\n");
    ret = -1;
    goto __end ;
  }

  status = atoi(pd);
  if (status!=mr__ok && status!=mr__need_sync) {
    printf("record status %d!\n",status);
    ret = -1;
    goto __end ;
  }

__end:
  drop_dbuffer(k);
  drop_dbuffer(v);

  return ret;
}

#if TEST_CASES==1
void test_myredis()
{
  struct myredis_s mr ;
  dbuffer_t res = alloc_default_dbuffer();

  myredis_init(&mr,"127.0.0.1",6379,"rds_example");

  myredis_write(&mr,"merchant_info","100","asdfjkasdlfj");
  myredis_read(&mr,"merchant_info","101",&res);
}
#endif
