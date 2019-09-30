#ifndef __FORMAT0_H__
#define __FORMAT0_H__


#include "dbuffer.h"
#include "list.h"


struct format0_s {
  dbuffer_t key ;
  dbuffer_t datetime;
  dbuffer_t pid;
  dbuffer_t log_level ;

  struct list_head list_item ;
} ;
typedef struct format0_s* format0_t ;



#endif /* __FORMAT0_H__*/
