#include <mysql.h>
#include <stdbool.h>
#include <string.h>
#include "log.h"
#include "kernel.h"
#include "mm_porting.h"
#include "mysqlwp.h"


int mysql_entry_init(mysql_entry_t e, const char *host,int port, 
     const char *usr, const char *pwd, const char *db)
{
  unsigned int proto = MYSQL_PROTOCOL_DEFAULT ;

  /* create and initialize MYSQL object */
  if (!(e->mysql=mysql_init(NULL))) {
    log_error("mysql_init() fail!\n");
    return -1;
  }

  /* set connection options */
  mysql_options(e->mysql, MYSQL_OPT_LOCAL_INFILE, 0);
  mysql_options(e->mysql, MYSQL_OPT_PROTOCOL, &proto);
  mysql_options(e->mysql, MYSQL_SET_CHARSET_NAME, "utf8");

  /* try connect server */
  if (!(mysql_real_connect(e->mysql, host, usr, pwd,
    db&&db[0]!='\0'?db:NULL, port,
    "/tmp/mysql.sock",
    0/*CLIENT_MULTI_STATEMENTS*/))
    ) {
    log_error("mysql_real_connect() fail(%s:%d, db:%s, user:%s)!\n",
        host,port,db,usr);
    return -1;
  }

  mysql_autocommit(e->mysql, true);

  return 0;
}

void mysql_entry_release(mysql_entry_t e)
{
  if (e->mysql) {
    mysql_close(e->mysql);
    e->mysql = NULL;
  }
}

int mysql_entry_reconnect(mysql_entry_t e, const char *host,int port, 
     const char *usr, const char *pwd, const char *db)
{
  mysql_entry_release(e);

  mysql_entry_init(e,host,port,usr,pwd,db);

  return 0;
}

int mysql_entry_get_alipay_channels(mysql_entry_t e, struct llist_head *res_list)
{
  const char *stmt = "select APP_ID, PRIVATE_KEY_PATH "
    " from CHANNEL_ALIPAY_CONFIGS "
    " where 1=1";


  if (unlikely(mysql_query(e->mysql, stmt))) {
    log_error("query stmt '%s' fail!\n",stmt);
    return -1;
  }

  if (e->myres) {
    mysql_free_result(e->myres);
  }

  e->myres = mysql_store_result(e->mysql) ;
  if (unlikely(!e->myres)) {
    log_error("mysql_store_result() fail!\n");
    return -1;
  }

  while (1) {
    char **res = mysql_fetch_row(e->myres);

    if (!res)
      break ;

    mch_res_item_t item = kmalloc(sizeof(struct merchant_res_item_s),0L);
    strncpy(item->app_id,res[0],sizeof(item->app_id));
    strncpy(item->priv_key_path,res[1],sizeof(item->priv_key_path));

    llist_add(&item->upper,res_list);
  }

  mysql_free_result(e->myres);
  e->myres = NULL ;

  return 0;
}

