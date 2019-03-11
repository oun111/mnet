#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "auto_id.h"

int aid_reset(auto_id_t id, char *name)
{
  id->val = 0L;

  strncpy(id->name,name,sizeof(id->name));

  snprintf(id->alias_val,sizeof(id->alias_val),
           "%5s_%.10ld",id->name,id->val);

  return 0;
}

char* aid_add_and_fetch(auto_id_t id,unsigned long v)
{
  id->val += v ;
  snprintf(id->alias_val,sizeof(id->alias_val),
           "%5s_%.10ld",id->name,id->val);

  return id->alias_val ;
}

unsigned long aid_add_and_fetch_long(auto_id_t id,unsigned long v)
{
  return id->val+=v ;
}

char* aid_fetch(auto_id_t id)
{
  return id->alias_val ;
}
