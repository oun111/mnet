#ifndef __MYSQLWP_H__
#define __MYSQLWP_H__

/*
 * mysqlwp -> mysql wrapper
 */
#include <limits.h>
#include "llist.h"

struct mysql_entry_s {
  void *mysql ;
  void *myres ;
} ;
typedef struct mysql_entry_s* mysql_entry_t ;

struct merchant_res_item_s {
  char app_id[64];
  char priv_key_path[PATH_MAX];
  struct llist_node upper ;
}__attribute__((__aligned__(64))) ;
typedef struct merchant_res_item_s* mch_res_item_t ;


extern int mysql_entry_init(mysql_entry_t e, const char *host,int port, 
     const char *usr, const char *pwd, const char *db);

extern void mysql_entry_release(mysql_entry_t e);

extern int mysql_entry_reconnect(mysql_entry_t e, const char *host,int port, 
     const char *usr, const char *pwd, const char *db);

extern int mysql_entry_get_alipay_channels(mysql_entry_t e, struct llist_head *res_list);

#endif /* __MYSQLWP_H__*/
