
CREATE TABLE `risk_control_configs` (
  `RCID` varchar(64) NOT NULL COMMENT '风控配置项标识/名称',
  `MAX_ORDERS` int(11) NOT NULL COMMENT '风控时间段内 支付宝通道  最大可支付订单 数量',
  `MAX_AMOUNT` decimal(24,2) NOT NULL COMMENT '风控时间段内支付宝通道  最大可支付金额',
  `PERIOD` int(11) NOT NULL COMMENT '风控时间段 周期，单位 秒',
  PRIMARY KEY (`RCID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8


CREATE TABLE `merchant_configs` (
  `ID` int(11) NOT NULL COMMENT 'paysvr商户配置项id，自增',
  `NAME` char(48) NOT NULL COMMENT 'paysvr商户名称，用于标识区分paysvr商户，应由 后台管理系统生成',
  `SIGN_TYPE` char(16) NOT NULL DEFAULT 'MD5' COMMENT 'paysvr商户请求验签算法，可用 md5，sha1，sha224，sha256，sha384，sha512',
  `PUBKEY` varchar(4096) DEFAULT NULL COMMENT '当签名算法为对称加密时，为 商户验签加签密钥; 当为 非对称加密时，该字段为 公钥',
  `PRIVKEY` varchar(4096) DEFAULT NULL COMMENT '当签名算法为对称加密时，该字段不使用; 当为 非对称加密时，该字段为 私钥',
  `PARAM_TYPE` char(16) NOT NULL DEFAULT 'html' COMMENT 'paysvr商户请求/响应/回调通知 格式，可选 json 或 html',
  `VERIFY_SIGN` int(11) NOT NULL DEFAULT 1 COMMENT '是否开启 商户 加签/验签 ，0 不开启，1 开启',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8


CREATE TABLE `order_data` (
  `ORDERID` varchar(32) NOT NULL COMMENT '唯一标识一个订单',
  `MCH_NO` varchar(32) NOT NULL COMMENT '产生订单的 paysvr商户号',
  `MCH_NOTIFY_URL` varchar(512) NOT NULL COMMENT '订单对应的paysvr商户异步通知url',
  `MCH_ORDERID` varchar(32) NOT NULL COMMENT 'paysvr商户提交的 商户订单id',
  `CHAN_NAME` varchar(32) NOT NULL COMMENT '订单对应的 支付宝通道 名称',
  `CHAN_MCH_NO` varchar(32) NOT NULL COMMENT '订单对应的 支付宝通道id，即app_id',
  `CHAN_MESSAGE` varchar(32) DEFAULT '' COMMENT '由支付宝平台 产生的 订单最终支付结果',
  `AMOUNT` decimal(16,2) NOT NULL COMMENT '订单 下单金额，单位 元',
  `STATUS` int(11) NOT NULL DEFAULT 0 COMMENT '订单状态，包括：0 未支付，1 正在支付，2 已支付，3 支付出错，4 超时',
  `USER_NOTIFY_STATUS` int(11) NOT NULL DEFAULT 0 COMMENT 'paysvr 给商户发送通知的结果，包括：0 未通知，1 正在通知，2 通知成功（商户已接收处理并返回success），3 通知失败',
  `CREATE_TIME` bigint(20) NOT NULL DEFAULT 0 COMMENT '订单创建时间，unix格式',
  PRIMARY KEY (`ORDERID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8

alter table `order_data` add index mch_orderid_index(`MCH_ORDERID`);


CREATE TABLE `channel_alipay_configs` (
  `id` int(11) NOT NULL AUTO_INCREMENT COMMENT '支付宝通道ID，自增',
  `name` char(48) NOT NULL COMMENT '支付宝通道名称，可自定义',
  `REQ_URL` varchar(512) NOT NULL COMMENT '支付宝通道支付请求发送URL，目前固定为 https://openapi.alipay.com/gateway.do ',
  `PARAM_TYPE` char(16) NOT NULL DEFAULT 'html' COMMENT '用于浏览器直接跳转的支付宝下单请求的格式，可选 json 或 html。仅用于调试',
  `PUBLIC_KEY_PATH` varchar(512) NOT NULL COMMENT '支付宝提供的验签公钥文件路径，pkcs1格式',
  `PRIVATE_KEY_PATH` varchar(512) NOT NULL COMMENT '商户生成的支付宝加签私钥路径，同时需要上传对应公钥到支付宝商户后台。pkcs1格式',
  `PRODUCT_CODE` char(64) NOT NULL COMMENT '支付宝支付请求参数，固定为 QUICK_WAP_PAY',
  `TIMEOUT_EXPRESS` char(16) NOT NULL COMMENT '支付宝支付请求有效时长，单位分钟，建议30～60m',
  `APP_ID` varchar(64) NOT NULL COMMENT '支付宝通道对应之app_id，唯一标识该支付宝通道，从支付宝商户后台获取',
  `METHOD` varchar(64) NOT NULL COMMENT '支付宝支付请求参数，固定为 alipay.trade.wap.pay',
  `FORMAT` varchar(32) NOT NULL COMMENT '支付宝支付请求参数，固定为 JSON',
  `CHARSET` varchar(16) NOT NULL COMMENT '支付宝支付请求参数，固定为utf-8',
  `SIGN_TYPE` varchar(16) NOT NULL COMMENT '支付宝支付请求参数，固定为RSA2',
  `VERSION` varchar(8) NOT NULL COMMENT '支付宝支付请求参数，固定为1.0',
  `NOTIFY_URL` varchar(512) NOT NULL COMMENT '用于接收支付宝发送的异步通知的url，默认为 paysvr 1366端口 对外url',
  `RETURN_URL` varchar(512) NOT NULL COMMENT '用于支付成功后app跳转的页面地址。目前不使用',
  `online` varchar(8) NOT NULL DEFAULT '1' COMMENT '标识当前支付宝通道的可用情况，1 可用，0 不可用',
  `RCID` varchar(64) NOT NULL COMMENT '当前支付宝通道对应的 风控规则id，见 风控配置表 定义',
  `ISTRANSFUND` int(11) NOT NULL DEFAULT 0 COMMENT '是否用于提现，0 否，1 是',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=7 DEFAULT CHARSET=utf8


CREATE TABLE `check_bill_details` (
  `ID` bigint(20) NOT NULL AUTO_INCREMENT COMMENT '单边账明细ID，自增',
  `MCH_ORDERID` varchar(32) NOT NULL default '' COMMENT 'paysvr商户提交的 商户订单id',
  `ERROR_TYPE` int(11) NOT NULL DEFAULT 0 COMMENT '单边类型，1 金额不等，2 账单缺流水，3 数据库缺流水',
  `ERROR_DESC` varchar(512) NOT NULL default '' COMMENT '错误详细描述',
  `MCH_DESC` varchar(96) NOT NULL default '' COMMENT '交易所名称，id，描述',
  `CHECK_DATE` date NOT NULL default now() COMMENT '订单生成日期',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB AUTO_INCREMENT=0 DEFAULT CHARSET=utf8



