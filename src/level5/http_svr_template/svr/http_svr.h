#ifndef __HTTP_SVR_H__
#define __HTTP_SVR_H__

extern ssize_t http_svr_rx_raw(Network_t net, connection_t pconn);

extern http_action_entry_t get_http_action_entry();

extern httpSvr_config_t get_current_configs();


extern void __http_svr_module_init(int argc, char *argv[]);

extern void __http_svr_module_exit();

#endif /* __HTTP_SVR_H__*/
