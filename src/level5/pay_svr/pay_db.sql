-- MySQL dump 10.16  Distrib 10.2.12-MariaDB, for Linux (x86_64)
--
-- Host: localhost    Database: pay_db
-- ------------------------------------------------------
-- Server version	10.2.12-MariaDB

/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET @OLD_CHARACTER_SET_RESULTS=@@CHARACTER_SET_RESULTS */;
/*!40101 SET @OLD_COLLATION_CONNECTION=@@COLLATION_CONNECTION */;
/*!40101 SET NAMES utf8 */;
/*!40103 SET @OLD_TIME_ZONE=@@TIME_ZONE */;
/*!40103 SET TIME_ZONE='+00:00' */;
/*!40014 SET @OLD_UNIQUE_CHECKS=@@UNIQUE_CHECKS, UNIQUE_CHECKS=0 */;
/*!40014 SET @OLD_FOREIGN_KEY_CHECKS=@@FOREIGN_KEY_CHECKS, FOREIGN_KEY_CHECKS=0 */;
/*!40101 SET @OLD_SQL_MODE=@@SQL_MODE, SQL_MODE='NO_AUTO_VALUE_ON_ZERO' */;
/*!40111 SET @OLD_SQL_NOTES=@@SQL_NOTES, SQL_NOTES=0 */;

--
-- Table structure for table `channel_alipay_configs`
--

DROP TABLE IF EXISTS `channel_alipay_configs`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `channel_alipay_configs` (
  `ID` int(11) NOT NULL DEFAULT 0,
  `NAME` char(48) NOT NULL,
  `REQ_URL` varchar(512) NOT NULL,
  `PARAM_TYPE` char(16) NOT NULL DEFAULT 'html',
  `PUBLIC_KEY_PATH` varchar(512) NOT NULL,
  `PRIVATE_KEY_PATH` varchar(512) NOT NULL,
  `PRODUCT_CODE` char(64) NOT NULL,
  `TIMEOUT_EXPRESS` char(16) NOT NULL,
  `APP_ID` varchar(64) NOT NULL,
  `METHOD` varchar(64) NOT NULL,
  `FORMAT` varchar(32) NOT NULL,
  `CHARSET` varchar(16) NOT NULL,
  `SIGN_TYPE` varchar(16) NOT NULL,
  `VERSION` varchar(8) NOT NULL,
  `NOTIFY_URL` varchar(512) NOT NULL,
  `RETURN_URL` varchar(512) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `channel_alipay_configs`
--

LOCK TABLES `channel_alipay_configs` WRITE;
/*!40000 ALTER TABLE `channel_alipay_configs` DISABLE KEYS */;
INSERT INTO `channel_alipay_configs` VALUES (0,'alipay','https://openapi.alipay.com/gateway.do','html','/home/user1/zyw/work/11ygalipay/ALI_rsa_public_key.2019032263662110.pem','/home/user1/zyw/work/11ygalipay/rsa_private_key.2019032263662110.pem','QUICK_WAP_PAY','5m','2019032263662110','alipay.trade.wap.pay','JSON','utf-8','RSA2','1.0','http://frpgz1.idcfengye.com:10047/alipay/notify','https://127.0.0.1/index'),(1,'alipay','https://openapi.alipay.com/gateway.do','html','/home/user1/zyw/work/11ygalipay/ALI_rsa_public_key.2019031163542035.pem','/home/user1/zyw/work/11ygalipay/rsa_private_key.2019031163542035.pem','QUICK_WAP_PAY','5m','2019031163542035','alipay.trade.wap.pay','JSON','utf-8','RSA2','1.0','http://frpgz1.idcfengye.com:10047/alipay/notify','https://127.0.0.1/index'),(2,'alipay','https://openapi.alipay.com/gateway.do','html','/home/user1/zyw/work/11ygalipay/ALI_rsa_public_key.2019032163594873.pem','/home/user1/zyw/work/11ygalipay/rsa_private_key.2019032163594873.pem','QUICK_WAP_PAY','5m','2019032163594873','alipay.trade.wap.pay','JSON','utf-8','RSA2','1.0','http://frpgz1.idcfengye.com:10047/alipay/notify','https://127.0.0.1/index');
/*!40000 ALTER TABLE `channel_alipay_configs` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `merchant_configs`
--

DROP TABLE IF EXISTS `merchant_configs`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `merchant_configs` (
  `ID` int(11) NOT NULL AUTO_INCREMENT,
  `NAME` char(48) NOT NULL,
  `SIGN_TYPE` char(16) NOT NULL DEFAULT 'MD5',
  `PUBKEY` varchar(4096) DEFAULT NULL,
  `PRIVKEY` varchar(4096) DEFAULT NULL,
  `PARAM_TYPE` char(16) NOT NULL DEFAULT 'html',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB AUTO_INCREMENT=3 DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `merchant_configs`
--

LOCK TABLES `merchant_configs` WRITE;
/*!40000 ALTER TABLE `merchant_configs` DISABLE KEYS */;
INSERT INTO `merchant_configs` VALUES (1,'mch_001','md5','123456789','','json'),(2,'mch_002','md5','absdfsd456','','json');
/*!40000 ALTER TABLE `merchant_configs` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `order_data`
--

DROP TABLE IF EXISTS `order_data`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `order_data` (
  `ORDERID` varchar(32) NOT NULL,
  `MCH_NO` varchar(32) NOT NULL,
  `MCH_NOTIFY_URL` varchar(512) NOT NULL,
  `MCH_ORDERID` varchar(32) NOT NULL,
  `CHAN_NAME` varchar(32) NOT NULL,
  `CHAN_MCH_NO` varchar(32) NOT NULL,
  `AMOUNT` decimal(16,2) NOT NULL,
  `STATUS` int(11) NOT NULL DEFAULT 0,
  `USER_NOTIFY_STATUS` int(11) NOT NULL DEFAULT 0,
  `CREATE_TIME` bigint(20) NOT NULL DEFAULT 0,
  PRIMARY KEY (`ORDERID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `order_data`
--

LOCK TABLES `order_data` WRITE;
/*!40000 ALTER TABLE `order_data` DISABLE KEYS */;
INSERT INTO `order_data` VALUES ('alp_00000000000000000006','mch_002','http://127.0.0.1:8080/epl_datong/myalipayNotifyResponse','100001','alipay','2019032163594873',0.01,0,0,1553677239687112),('alp_00000000000000000007','mch_002','http://127.0.0.1:8080/epl_datong/myalipayNotifyResponse','100002','alipay','2019031163542035',0.01,0,0,1553677279054402),('alp_00000000000000000008','mch_002','http://127.0.0.1:8080/epl_datong/myalipayNotifyResponse','100003','alipay','2019032263662110',0.01,0,0,1553677287221598),('alp_00000000000000000009','mch_001','http://127.0.0.1:8080/epl_datong/myalipayNotifyResponse','1625363905-34379','alipay','2019032163594873',0.01,0,0,1553678065068159),('alp_00000000000000000010','mch_001','http://127.0.0.1:8080/epl_datong/myalipayNotifyResponse','1625363905-34395','alipay','2019031163542035',0.01,0,0,1553678226518122),('alp_00000000000000000011','mch_001','http://127.0.0.1:8080/epl_datong/myalipayNotifyResponse','1625363905-34467','alipay','2019032263662110',0.01,2,2,1553678894724994),('alp_00000000000000000012','mch_001','http://127.0.0.1:8080/epl_datong/myalipayNotifyResponse','1625363905-34633','alipay','2019032163594873',0.10,2,2,1553680513154726),('alp_00000000000000000013','mch_001','http://127.0.0.1:8080/epl_datong/myalipayNotifyResponse','1625363905-34641','alipay','2019031163542035',0.20,2,2,1553680585553438),('alp_00000000000000000014','mch_001','http://127.0.0.1:8080/epl_datong/myalipayNotifyResponse','1625363905-34649','alipay','2019032263662110',0.30,2,2,1553680643914200),('alp_00000000000000000015','mch_001','http://127.0.0.1:8080/epl_datong/huitongAlih5payNotifyResponse','1625363905-34796','alipay','2019032163594873',0.01,2,2,1553682082431015);
/*!40000 ALTER TABLE `order_data` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `risk_control_configs`
--

DROP TABLE IF EXISTS `risk_control_configs`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `risk_control_configs` (
  `CHANNEL` varchar(64) NOT NULL,
  `MAX_ORDERS` int(11) NOT NULL,
  `MAX_AMOUNT` decimal(24,2) NOT NULL,
  `PERIOD` int(11) NOT NULL,
  PRIMARY KEY (`CHANNEL`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `risk_control_configs`
--

LOCK TABLES `risk_control_configs` WRITE;
/*!40000 ALTER TABLE `risk_control_configs` DISABLE KEYS */;
INSERT INTO `risk_control_configs` VALUES ('alipay',9999,11.20,60);
/*!40000 ALTER TABLE `risk_control_configs` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Dumping routines for database 'pay_db'
--
/*!40103 SET TIME_ZONE=@OLD_TIME_ZONE */;

/*!40101 SET SQL_MODE=@OLD_SQL_MODE */;
/*!40014 SET FOREIGN_KEY_CHECKS=@OLD_FOREIGN_KEY_CHECKS */;
/*!40014 SET UNIQUE_CHECKS=@OLD_UNIQUE_CHECKS */;
/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
/*!40111 SET SQL_NOTES=@OLD_SQL_NOTES */;

-- Dump completed on 2019-03-27 18:23:05
