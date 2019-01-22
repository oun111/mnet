#ifndef __L4_H__
#define __L4_H__

#include "connection.h"


extern int tcp_rx(Network_t net, connection_t pconn);

extern int tcp_tx(Network_t net, connection_t pconn);

extern int tcp_accept(Network_t net, connection_t pconn);

extern void tcp_close(Network_t net, connection_t pconn);

#endif /* __L4_H__*/
