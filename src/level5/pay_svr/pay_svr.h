
#ifndef __PAY_SVR_H__
#define __PAY_SVR_H__

extern paySvr_config_t get_running_configs();

extern backend_entry_t get_backend_entry();

extern pay_channels_entry_t get_pay_channels_entry();

extern order_entry_t get_order_entry();

//extern rds_order_entry_t get_rds_order_entry();

extern merchant_entry_t get_merchant_entry();

//extern myredis_t get_myredis();

#endif /* __PAY_SVR_H__*/
