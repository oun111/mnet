
#ifndef __CONNECTION_H__
#define __CONNECTION_H__

#include <stdlib.h>
#include <sys/epoll.h>
#include "ssl.h"
#include "proto.h"
#include "dbuffer.h"
#include "list.h"
#include "mm_porting.h"
#include "objpool.h"

/**
 * connection object
 */
struct __attribute__((__aligned__(64))) connection_s {

  int fd ;

  proto_opt l4opt ;

  proto_opt l5opt ;

  dbuffer_t txb ;

  dbuffer_t rxb ;

  int is_close ;

  struct list_head pool_item ;

  int module_id ;

  ssl_item_t ssl ;

  struct list_head active_item;

  long long last_active ; // last active time

} ;



struct __attribute__((__aligned__(64))) Network_s {

  int m_efd ;

  objPool_t pool ;

  #define MAXEVENTS  128 /* 1024*/
  struct epoll_event elist[MAXEVENTS];

  struct active_conn_s {
    struct list_head list ;
    pthread_rwlock_t lck ;
  } active ;

  connection_t (*reg_local)(Network_t,int fd,int mod_id);

  connection_t (*reg_inbound)(Network_t,int fd,int mod_id);

  connection_t (*reg_outbound)(Network_t,int fd,int mod_id);

  int (*unreg_all)(Network_t,connection_t);

} ;



extern int init_conn_pool(Network_t net, ssize_t pool_size);

extern void release_conn_pool(Network_t net);

extern connection_t alloc_conn(Network_t net, int fd, proto_opt *l4opt, 
                               proto_opt *l5opt, bool bSSL, bool markActive);

//extern void save_conn_fd(connection_t, int fd);

extern int free_conn(Network_t net, connection_t pconn);

extern int scan_timeout_connections(Network_t net, int tos);

extern void update_connection_times(connection_t pconn);

#endif /* __CONNECTION_H__*/
