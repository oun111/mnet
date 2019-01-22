
#ifndef __MP2_H__
#define __MP2_H__

#include "connection.h"
#include "proto.h"


#define __VER_STR__  "1.0.1"



extern int mp2_pre_init(int argc, char *argv[]);

extern int mp2_init(Network_t net);

extern void mp2_release();

extern int mp2_rx(Network_t net, connection_t pconn);

extern int mp2_tx(Network_t net, connection_t pconn);

extern int mp2_greeting(Network_t net, connection_t pconn);

extern void mp2_close(Network_t net, connection_t pconn);

extern int mp2_backend_rx(Network_t net, connection_t pconn);

extern int mp2_backend_tx(Network_t net, connection_t pconn);

extern void mp2_backend_close(Network_t net, connection_t pconn);

extern int mp2_backend_init(Network_t net);

extern void mp2_backend_release();

extern int mp2_backend_com_query(Network_t net, connection_t pconn, 
    const char *inb, const size_t sz, const int ptn_pos) ;

#endif /* __MP2_H__*/
