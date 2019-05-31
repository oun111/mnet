#include "connection.h"
#include "kernel.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include "log.h"


static void init_active_lock(Network_t net)
{
  //pthread_rwlock_init(&net->active.lck,NULL);

  pthread_mutex_init(&net->active.lck,NULL);
}

static void release_active_lock(Network_t net)
{
  //pthread_rwlock_destroy(&net->active.lck);

  pthread_mutex_destroy(&net->active.lck);
}

static int try_lock_active_conn(Network_t net)
{
  //return pthread_rwlock_trywrlock(&net->active.lck); 

  return pthread_mutex_trylock(&net->active.lck); 
}

static void lock_active_conn(Network_t net)
{
  //pthread_rwlock_wrlock(&net->active.lck);

  pthread_mutex_lock(&net->active.lck);
}

static void unlock_active_conn(Network_t net)
{
  //pthread_rwlock_unlock(&net->active.lck);

  pthread_mutex_unlock(&net->active.lck);
}

static
void add_to_active_list(Network_t net, connection_t pconn)
{
  net->active.lock(net);

  list_add(&pconn->active_item,&net->active.list);

  net->active.unlock(net);
}

static
void remove_from_active_list(Network_t net, connection_t pconn)
{
  net->active.lock(net);

  list_del(&pconn->active_item);

  net->active.unlock(net);
}

static
void init_active_list(Network_t net)
{
  init_active_lock(net);

  net->active.try_lock= try_lock_active_conn ;
  net->active.lock    = lock_active_conn ;
  net->active.unlock  = unlock_active_conn ;

  // the active list
  INIT_LIST_HEAD(&net->active.list);
}

static
void release_active_list(Network_t net)
{
  release_active_lock(net);
}

int scan_timeout_conns(void *pnet, void *ptos)
{
  connection_t pos,n ;
  long long curr = time(NULL);
  Network_t net = (Network_t)pnet;
  const int tos = (int)(uintptr_t)ptos ;


  if (net->active.try_lock(net)) 
    return -1;

  list_for_each_entry_safe(pos,n,&net->active.list,active_item) {

    if ((curr-pos->last_active)>tos) {
      //log_info("closing idle fd %d\n",pos->fd);
      shutdown(pos->fd,SHUT_RDWR);
      continue;
    }

    net->active.unlock(net);

    if (net->active.try_lock(net)) 
      return -1;
  }

  net->active.unlock(net);

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

  reset_dbuffer(pconn->txb);
  reset_dbuffer(pconn->rxb);

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

  close(pconn->fd);

  obj_pool_free(net->pool,pconn);

  return 0;
}

