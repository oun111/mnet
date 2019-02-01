#ifndef __EXAMPLE_HTTP_H__
#define __EXAMPLE_HTTP_H__

#include "pay_action.h"
#include "backend.h"


extern ssize_t pay_svr_http_rx(Network_t net, connection_t pconn);

extern backend_entry_t get_backend_entry();

extern pay_action_entry_t get_pay_action_entry();

#endif /* __EXAMPLE_HTTP_H__*/
