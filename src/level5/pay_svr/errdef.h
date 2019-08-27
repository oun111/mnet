#ifndef __ERRDEF_H__
#define __ERRDEF_H__

enum err_codes {
  E_RSA,
  E_BASE64,        
  E_NO_BODY,       
  E_NO_SUBJECT,    
  E_NO_RETURL,     
  E_NO_OTN,        
  E_NO_OBN,        
  E_NO_MCHID,      
  E_NO_NOTIFYURL,  
  E_NO_SIGN,       
  E_NO_PAYEE_ACCT, 
  E_NO_PAYER_NAME, 
  E_NO_PAYEE_NAME, 
  E_NO_AMT,        
  E_NO_UPDATE_TBL, 
  E_NO_CONF_KEY, 
  E_NO_CONF_REQURL,
  E_DUP_ORDERID,    
  E_DUP_OTN,       
  E_BAD_SIGN,       
  E_BAD_MCH,        
  E_BAD_ROUTE,      
  E_NO_ORDER_BY_ID, 
  E_NO_ORDER_BY_OTN,
  E_SAVE_ORDER,     
  E_PUB_REDIS,       
  // TODO: add new error code here
} ;

const char* err_msgs[] = 
{
  "RSA签名错误",
  "BASE64编码错误",
  "缺少\'body\'字段",
  "缺少\'subject\'字段",
  "缺少\'return_url\'字段",
  "缺少\'out_trade_no\'字段",
  "缺少\'out_biz_no\'字段",
  "缺少\'mch_id\'字段",
  "缺少\'notify_url\'字段",
  "缺少\'sign\'字段",
  "缺少\'payee_account\'字段",
  "缺少\'payer_show_name\'字段",
  "缺少\'payee_real_name\'字段",
  "缺少\'total_amount\'字段",
  "缺少\'update_table\'字段",
  "配置错误：缺少密钥",
  "配置错误：缺少支付宝请求地址",
  "重复的平台订单号\'%s\'",
  "重复的商户订单号\'%s\'",
  "无效签名",
  "商户号\'%s\'无效",
  "无可用的\'%s\'渠道",
  "找不到平台订单号\'%s\'",
  "找不到商户订单号\'%s\'",
  "保存订单\'%s\'失败",
  "推送消息到redis失败",
};

#endif /* __ERRDEF_H__*/

