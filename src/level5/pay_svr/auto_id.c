#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "dbuffer.h"
#include "myredis.h"
#include "auto_id.h"



#define ALIAS_FMT(__id) do{ \
  snprintf((__id)->alias_val,sizeof((__id)->alias_val), \
           "%.5s_%.20lld",(__id)->id_name,(__id)->val); \
} while(0)

int aid_init(auto_id_t id, char *name, void *rds_handle)
{
  id->val = 0LL;
  strncpy(id->id_name,name,sizeof(id->id_name));

  ALIAS_FMT(id);

  if (rds_handle) {
    id->myrds_handle = rds_handle ;
  }

  return 0;
}

void aid_reset(auto_id_t id)
{
  if (id) {
    id->val = 0LL ;
    myredis_reset((myredis_t)id->myrds_handle,ot_var);
    ALIAS_FMT(id);
  }
}

char* aid_add_and_fetch(auto_id_t id,unsigned long v)
{
  if (!id || myredis_add_and_fetch((myredis_t)id->myrds_handle,&id->val)) {
    id->val += v ;
  }

  ALIAS_FMT(id);

  return id->alias_val ;
}

char* aid_fetch(auto_id_t id)
{
  return id->alias_val ;
}
