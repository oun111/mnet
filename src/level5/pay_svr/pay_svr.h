#ifndef __EXAMPLE_HTTP_H__
#define __EXAMPLE_HTTP_H__

#include "pay_action.h"


extern ssize_t pay_svr_http_rx(Network_t net, connection_t pconn);

extern void register_pay_action(pay_action_t);


#endif /* __EXAMPLE_HTTP_H__*/
