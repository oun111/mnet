#include "connection.h"
#include "kernel.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "log.h"


int init_conn_pool(Network_t net, ssize_t pool_size)
{
  connection_t pos,n ;


  net->pool = create_obj_pool("normal-connection-pool",
                              pool_size,struct connection_s);

  list_for_each_objPool_item(pos,n,net->pool) {
    pos->txb = alloc_default_dbuffer();
    pos->rxb = alloc_default_dbuffer();
  }

  log_info("connection pool size: %zd\n",net->pool->pool_size);

  return 0;
}

void release_conn_pool(Network_t net)
{
  connection_t pos,n ;

  list_for_each_objPool_item(pos,n,net->pool) {
    drop_dbuffer(pos->txb);
    drop_dbuffer(pos->rxb);
  }

  release_obj_pool(net->pool,struct connection_s);
}

connection_t alloc_conn(Network_t net, int fd, proto_opt *l4opt,
                        proto_opt *l5opt, bool bSSL)
{
  connection_t pconn = obj_pool_alloc(net->pool,struct connection_s);


  if (!pconn) {
    pconn = obj_pool_alloc_slow(net->pool,struct connection_s);
    if (pconn) {
      pconn->txb = alloc_default_dbuffer();
      pconn->rxb = alloc_default_dbuffer();
    }
  }

  if (!pconn) {
    log_error("fatal: cant allocate more connections\n");
    return NULL ;
  }

  /* init the proto opts */
  register_proto_opt(pconn,0,l4opt) ;
  register_proto_opt(pconn,1,l5opt) ;

  pconn->fd = fd;

  pconn->is_close = 0;

  pconn->module_id = -1;

  pconn->ssl = NULL;

  if (bSSL) {
    pconn->ssl = ssl_init();

    ssl_connect(pconn->ssl,fd);
  }

  return pconn ;
}

int free_conn(Network_t net, connection_t pconn)
{
  pconn->is_close = 1;

  obj_pool_free(net->pool,pconn);

  return 0;
}

