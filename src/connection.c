#include "connection.h"
#include "kernel.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "log.h"
#include "socket.h"


int scan_timeout_conns(void *pnet, void *ptos)
{
  connection_t pos,n ;
  long long curr = time(NULL);
  Network_t net = (Network_t)pnet;
  const int tos = (int)(uintptr_t)ptos ;


  list_for_each_entry_safe(pos,n,&net->active.list,active_item) {

    if ((curr-pos->last_active)<=tos) 
      break ;

    log_info("closing idle fd %d\n",pos->fd);
    sock_close(pos->fd);
  }

  return 0;
}

void update_conn_times(Network_t net, connection_t pconn)
{
  if (list_empty(&pconn->active_item))
    return ;

  pconn->last_active = time(NULL);

  list_del(&pconn->active_item);
  list_add_tail(&pconn->active_item,&net->active.list);
}

int init_conn_pool(Network_t net, ssize_t pool_size)
{
  connection_t pos,n ;


  INIT_LIST_HEAD(&net->active.list);

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

  if (!net || !net->pool) {
    return ;
  }

  list_for_each_objPool_item(pos,n,net->pool) {
    drop_dbuffer(pos->txb);
    drop_dbuffer(pos->rxb);
  }

  release_obj_pool(net->pool,struct connection_s);

  //release_active_list(net);
}

connection_t alloc_conn(Network_t net, int fd, proto_opt *l4opt,
                        proto_opt *l5opt, bool bSSL, bool markActive)
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

  //pconn->last_active = 0;
  pconn->last_active = time(NULL);

  reset_dbuffer(pconn->txb);
  reset_dbuffer(pconn->rxb);

  if (likely(markActive==true))
    list_add_tail(&pconn->active_item,&net->active.list);
  else 
    INIT_LIST_HEAD(&pconn->active_item);

  if (bSSL) {
    pconn->ssl = ssl_init();

    ssl_connect(pconn->ssl,fd);
  }

  return pconn ;
}

int free_conn(Network_t net, connection_t pconn)
{
  //remove_from_active_list(net,pconn);
  list_del(&pconn->active_item);

  pconn->is_close = 1;

  close(pconn->fd);

  obj_pool_free(net->pool,pconn);

  return 0;
}

