#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "dbuffer.h"
#include "myredis.h"
#include "auto_id.h"

int aid_init(auto_id_t id, char *name, void *rds_handle)
{
  id->val = 0LL;

  strncpy(id->id_name,name,sizeof(id->id_name));
  snprintf(id->cache_name,sizeof(id->cache_name),"%s_cache",name);
  snprintf(id->mq_name,sizeof(id->mq_name),"%s_mq",name);
  snprintf(id->var_name,sizeof(id->var_name),"%s_var",name);

  snprintf(id->alias_val,sizeof(id->alias_val),
           "%5s_%.10lld",id->id_name,id->val);

  if (rds_handle) {
    id->myrds_handle = rds_handle ;
    //myredis_reset((myredis_t)id,ot_var);
  }

  return 0;
}

void aid_reset(auto_id_t id)
{
  if (id) {
    id->val = 0LL ;
    myredis_reset((myredis_t)id,ot_var);
    snprintf(id->alias_val,sizeof(id->alias_val),
             "%5s_%.10lld",id->id_name,id->val);
  }
}

char* aid_add_and_fetch(auto_id_t id,unsigned long v)
{
  if (!id || myredis_add_and_fetch((myredis_t)id,&id->val)) {
    id->val += v ;
  }

  snprintf(id->alias_val,sizeof(id->alias_val),
           "%5s_%.10lld",id->id_name,id->val);

  return id->alias_val ;
}

char* aid_fetch(auto_id_t id)
{
  return id->alias_val ;
}
