#include "connection.h"
#include "kernel.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include "log.h"




static
void add_to_active_list(Network_t net, connection_t pconn)
{
  pthread_rwlock_wrlock(&net->active.lck);

  list_add(&pconn->active_item,&net->active.list);

  pthread_rwlock_unlock(&net->active.lck);
}

static
void remove_from_active_list(Network_t net, connection_t pconn)
{
  pthread_rwlock_wrlock(&net->active.lck);

  list_del(&pconn->active_item);

  pthread_rwlock_unlock(&net->active.lck);
}

static
void init_active_list(Network_t net)
{
  pthread_rwlock_init(&net->active.lck,NULL);

  // the active list
  INIT_LIST_HEAD(&net->active.list);
}

static
void release_active_list(Network_t net)
{
  pthread_rwlock_destroy(&net->active.lck);
}

int scan_timeout_conns(void *pnet, void *ptos)
{
  connection_t pos, n;
  long long curr = time(NULL);
  Network_t net = (Network_t)pnet;
  const int tos = (int)(uintptr_t)ptos ;
  static int sec = 0;


  if (sec++ < tos)
    return 0;

  if (pthread_rwlock_tryrdlock(&net->active.lck)) 
    return -1;

  list_for_each_entry_safe(pos,n,&net->active.list,active_item) {

    pthread_rwlock_unlock(&net->active.lck);

    // check for timeout connection(s)
    if ((curr-pos->last_active)>tos) {
      shutdown(pos->fd,SHUT_RDWR);
      log_debug("disconnect idle client %d\n",pos->fd);
    }

    if (pthread_rwlock_tryrdlock(&net->active.lck)) 
      return -1;
  }

  pthread_rwlock_unlock(&net->active.lck);

  sec = 0;

  return 0;
}

void update_conn_times(connection_t pconn)
{
  pconn->last_active = time(NULL);
}

int init_conn_pool(Network_t net, ssize_t pool_size)
{
  connection_t pos,n ;


  init_active_list(net);

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

  release_active_list(net);
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

  pconn->last_active = 0;

  if (markActive==true) {
    add_to_active_list(net,pconn);
  }

  if (bSSL) {
    pconn->ssl = ssl_init();

    ssl_connect(pconn->ssl,fd);
  }

  return pconn ;
}

int free_conn(Network_t net, connection_t pconn)
{
  remove_from_active_list(net,pconn);

  pconn->is_close = 1;

  obj_pool_free(net->pool,pconn);

  return 0;
}

