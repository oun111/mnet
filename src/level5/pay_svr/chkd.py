#!/usr/bin/python3

import json
import pymysql
import web  
import re
import os
import time
import datetime
import getopt
import decimal
import sys
from Crypto.PublicKey import RSA
from Crypto.Signature import PKCS1_v1_5
from Crypto.Hash import SHA256
from base64 import b64encode,b64decode
#from base64 import decodebytes,encodebytes
import urllib
import zipfile
import chardet
import csv
import codecs
import logging
import logging.handlers


appname = "chkd"
logger = logging.getLogger((appname + '_log'))


class mysql_configs(object):
  address = "127.0.0.1"
  port    = 3306
  db      = ""
  usr     = ""
  pwd     = ""
  odrTbl  = ""
  mchTbl  = ""
  chanTbl = ""
  cbTbl   = ""
  cbmTbl  = ""


class Mysql(object):
  def __init__(self,host,port,db,usr,pwd):
    self.host = host
    self.port = port
    self.db   = db
    self.usr  = usr
    self.pwd  = pwd

    self.do_connect()


  def do_connect(self):
    self.m_conn = pymysql.connect(host=self.host,port=self.port,user=self.usr,
                                  password=self.pwd,database=self.db,autocommit=False,
                                  cursorclass=pymysql.cursors.DictCursor)


  def do_exec(self,sql):

    #logger.debug("sql: " + strsql)

    while True:
      try:
        self.m_conn.commit()

        with self.m_conn.cursor() as cs:
          cs.execute(sql)
          res = cs.fetchall()

        self.m_conn.commit()

        return res

      except pymysql.OperationalError as e:
        self.do_connect()


  def query(self,table,selist=" * ",cond=""):
    strsql = ("select " + selist + " from " + table + " " + cond)

    return self.do_exec(strsql)


  def update(self,strSql):

    self.do_exec(strSql)
    

class simple_tools:
  @staticmethod
  def dateStr_to_microSecs(date):

    tstruct = time.strptime(date,"%Y-%m-%d")
    if tstruct==None:
      logger.debug("invalid date: "+date)
      return -1

    return (time.mktime(tstruct)+2208988800)*1000000

  @staticmethod
  def get_nextDate_microSecs(ds):
    return (ds + 86400*1000000)

  @staticmethod
  def to_mysql_in_list(lst):
    chanlist= ""
    logger.debug(lst)

    for i in range(len(lst)):
      if i>0:
        chanlist = chanlist + ','
      chanlist = chanlist + '\'' + lst[i] + '\''

    return chanlist

  @staticmethod
  def RSAwithSHA256_sign(privkey, signtmp):
    signer = PKCS1_v1_5.new(privkey)
    signature = signer.sign(SHA256.new(signtmp.encode('utf-8')))
    #sign = encodebytes(signature).decode("utf8").replace("\n", "")
    sign = b64encode(signature).decode("utf8").replace("\n", "")
    return sign


class check_bill_biz:
  def __init__(self,cfg_contents):


    self.mysql_cfg  = mysql_configs()
    mscfg = self.mysql_cfg

    #
    # update mysql configs
    # 
    gcfg = json.loads(cfg_contents)
    cfg  = gcfg['Globals']['Mysql']
    if (len(cfg)>0):
      mscfg.address = cfg['address']
      mscfg.port    = cfg['port']
      mscfg.db      = cfg['db']
      mscfg.usr     = cfg['usr']
      mscfg.pwd     = cfg['pwd']
      mscfg.odrTbl  = cfg['orderTableName']
      mscfg.cbTbl   = cfg['cbTableName']
      mscfg.cbmTbl  = cfg['cbmTableName']
      mscfg.mchTbl  = cfg['merchantConfigTableName']
      mscfg.chanTbl = cfg['alipayConfigTableName']
      logger.debug("mysql configs: host: {0}:{1}".
            format(mscfg.address,mscfg.port))

    self.m_mysql = Mysql(mscfg.address,mscfg.port,mscfg.db,
                         mscfg.usr,mscfg.pwd)


    cbcfg = gcfg['Globals']['CheckBill']
    if (len(cbcfg)>0):
      self.cb_path = cbcfg['billFilePath']
      self.cb_port = cbcfg['listenPort']


  def get_listen_port(self):
    return self.cb_port


  def get_download_link(self,r,bill_date):

    pay_params  = {}
    biz_content = {}
    app_id = r['APP_ID']
    pay_params['app_id'] = app_id
    pay_params['method'] = 'alipay.data.dataservice.bill.downloadurl.query'
    pay_params['format'] = 'JSON'
    pay_params['charset']= 'utf-8'
    pay_params['sign_type']= 'RSA2'
    pay_params['timestamp']= datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')
    pay_params['version']= '1.0'

    biz_content['bill_type']= 'signcustomer'  #'trade'
    biz_content['bill_date']= bill_date

    pay_params['biz_content'] = json.dumps(biz_content)

    # building pre sign string
    sort_list = sorted(pay_params.items())
    sort_str  = "&".join("{0}={1}".format(k, v) for k, v in sort_list)

    privkey = ""
    with open(r['PRIVATE_KEY_PATH']) as fp:
      privkey = RSA.importKey(fp.read())

    pay_params['sign'] = simple_tools.RSAwithSHA256_sign(privkey,sort_str)
    sort_list = sorted(pay_params.items())

    # final request string
    sort_str  = "&".join("{0}={1}".format(k, urllib.parse.quote_plus(v)) for k, v in sort_list)
    reqstr = (r['REQ_URL'] + "?" + sort_str)

    #logger.debug("final request string: {0}".format(reqstr))

    req = urllib.request.Request(reqstr)
    res = urllib.request.urlopen(req)
    res = res.read()

    pr = json.loads(res)
    lnk= pr['alipay_data_dataservice_bill_downloadurl_query_response'].get('bill_download_url')

    if lnk==None:
      reason = pr['alipay_data_dataservice_bill_downloadurl_query_response']['sub_msg']
      logger.debug("get download link by app_id '{0}' bill-date '{1}' fail: {2}"
            .format(app_id,bill_date,reason))

    return lnk 



  def download_check_file(self,appid,basepath,bd,lnk):
    #logger.debug("trying link: " + lnk)

    if not os.path.exists(basepath):
      os.makedirs(basepath)

    req = urllib.request.Request(lnk)
    res = urllib.request.urlopen(req)
    res = res.read()

    zfpath = (basepath+"/"+appid+"_"+bd+".zip")

    with open(zfpath, "wb") as f: 
      f.write(res)

    zf = zipfile.ZipFile(zfpath)

    for xf in zf.namelist():
      # zip 文件默认先转为 cp437 编码
      #logger.debug("xf: " + xf.encode(encoding=u'cp437',errors='ignore').decode(u'gb2312'))
      xf_info = zf.getinfo(xf)
      xf_path = (basepath+"/"+appid+"_"+(xf.encode(encoding=u'cp437',errors='ignore').decode(u'gb2312')))

      with open(xf_path,"wb") as f:
        xff = zf.open(xf_info)
        f.write(xff.read())

    os.remove(zfpath)


  def get_db_order_details(self,bill_date,appid_list):

    db_dict = {}
    chanlist= simple_tools.to_mysql_in_list(appid_list)


    prev_date = simple_tools.dateStr_to_microSecs(bill_date)
    next_date = simple_tools.get_nextDate_microSecs(prev_date)

    # check db details by mch names
    odr_rows = self.m_mysql.query(self.mysql_cfg.odrTbl," * ",
                              " where create_time>={0} and create_time<={1} and "\
                              " status=2 and chan_mch_no in({2})".format(prev_date,next_date,chanlist))

    for odr in odr_rows:
      db_dict[odr['MCH_ORDERID']] = (float(odr['AMOUNT']),odr['MCH_NO'],odr['CHAN_MCH_NO'])

    return db_dict


  def get_check_file_details(self,basepath,transfund,appid_group):

    cf_dict = {}

    for __path,__subPaths,__files in os.walk(basepath):

      for f in __files:

        ptn = ".*业务明细.csv"
        if transfund==1:
          ptn = ".*账务明细.csv"

        if bool(re.match(ptn,f))!=True :
          continue

        if appid_group.get(f.split('_')[0])==None:
          continue

        #print("parsing csv " + f)

        with codecs.open(basepath+"/"+f,'r','gb18030') as fcsv:
          fc = csv.reader(fcsv)

          for r in fc:

            if len(r[0])==0 or r[0][0]=='#' or r[0][0:6].isdigit()==False:
              continue

            amount = 0.0
            sid = ''
            if transfund==0:
              sid = r[1][:-1]
              amount = float(r[12])
            else:
              sid = r[2][:-1]
              amount = abs(float(r[7]))

            # skip '\t' at last position
            cf_dict[sid] = (amount,r[3])

    return cf_dict


  def errStr(self,err):
    if err==1:
      return '金额不等'
    elif err==2:
      return '账单缺流水'
    elif err==3:
      return '数据库缺流水'
    return '未知错误类型: {0}'.format(err)


  def save_check_bill_details(self,k,v,e,bd):
    self.m_mysql.update(" insert into {v0} (MCH_ORDERID,ERROR_TYPE,ERROR_DESC,MCH_DESC,BILL_DATE) "\
                        " values('{v1}',{v2},'{v3}','{v4}','{v5}')"
                        .format(v0=self.mysql_cfg.cbTbl,v1=k,v2=e,v3=self.errStr(e),v4=v[1],v5=bd)
                       )

  def save_check_bill_summary(self,mch,amt,transfund,res,bd):
    self.m_mysql.update(" insert into {v0} (MCH_DESC,TOTAL_AMOUNT,ISTRANSFUND,SUCCESS,BILL_DATE) "\
                        " values('{v1}',{v2},{v3},'{v4}','{v5}')"
                        .format(v0=self.mysql_cfg.cbmTbl,v1=mch,v2=amt,v3=transfund,v4=res,v5=bd)
                       )


  def check_details(self,bill_date,basepath,transfund,appid_group):

    err = 0
    db_dict = self.get_db_order_details(bill_date,list(appid_group.keys()))
    #logger.debug("orders: {0}".format(db_dict))

    cf_dict = self.get_check_file_details(basepath,transfund,appid_group)
    #logger.debug("orders: {0}".format(cf_dict))

    db_dict_copy = db_dict.copy()

    for ckey in cf_dict.keys():
      db_ret = db_dict.get(ckey)

      if db_ret==None:
        continue

      amt = db_ret[0]

      if amt!=cf_dict[ckey][0]:
        self.save_check_bill_details(ckey,cf_dict[ckey],1,bill_date)
        logger.debug("error1: amount no equal: {0}".format(ckey))
        err = 1
        continue

      db_dict.pop(ckey)


    for dkey in db_dict_copy.keys():
      if cf_dict.get(dkey)!=None:
        cf_dict.pop(dkey)


    for k in db_dict.keys():
      self.save_check_bill_details(k,db_dict[k],2,bill_date)
      logger.debug("error2: check none, db has: {0}".format(k))
      err = 1


    for k in cf_dict.keys():
      self.save_check_bill_details(k,cf_dict[k],3,bill_date)
      logger.debug("error3: check has, db none: {0}".format(k))
      err = 1

    logger.debug("details checking done!")

    return err,db_dict_copy



  def check_main(self,bill_date,err,db_dict,transfund,appid_group):

    if (err==1):
      self.save_check_bill_summary('all merchants',0.0,transfund,-1,bill_date)
      return

    prev_date = simple_tools.dateStr_to_microSecs(bill_date)
    next_date = simple_tools.get_nextDate_microSecs(prev_date)

    chanlist  = simple_tools.to_mysql_in_list(list(appid_group.keys()))

    rows = self.m_mysql.query(self.mysql_cfg.mchTbl," * "," where 1=1 ")
    for r in rows:
      mch = r['NAME']
      amt = 0.0

      for v in db_dict.values():
        if v[1]==mch:
          amt = amt + v[0]

      self.save_check_bill_summary(mch,amt,transfund,1,bill_date)



  def do_biz(self,bill_date,transfund):
    rows = self.m_mysql.query(self.mysql_cfg.chanTbl," * "," where istransfund={0} ".format(transfund))

    basepath = self.cb_path + "alipay_bills/" + bill_date + "/"

    appid_group = {}

    for r in rows:
      lnk  = self.get_download_link(r,bill_date)
      #lnk = None
      appid= r['APP_ID']

      appid_group[appid] = 1

      if lnk==None:
        continue

      self.download_check_file(appid,basepath,bill_date,lnk)


    # check order details
    err,db_dict = self.check_details(bill_date,basepath,transfund,appid_group)

    # check main
    self.check_main(bill_date,err,db_dict,transfund,appid_group)



  
class cb_web_req:  
  def GET(self, bill_date):  
    # in money
    cbbiz.do_biz(bill_date,0)
    # out money
    cbbiz.do_biz(bill_date,1)
    return 'check bill on date {0} success!'.format(bill_date)  


def init_log(logpath):
    logger.setLevel(logging.DEBUG)

    #fh = logging.FileHandler("/tmp/{cn}.log".format(appname))

    formatter = logging.Formatter('%(asctime)s - %(module)s(%(process)d)%(funcName)s.%(lineno)d - %(levelname)s - %(message)s')

    if (len(logpath)>0):
      fh = logging.handlers.TimedRotatingFileHandler((logpath+"/"+appname+".log"),when='D')
      fh.suffix = "%Y-%m-%d.log"
      fh.setLevel(logging.DEBUG)
      fh.setFormatter(formatter)
      logger.addHandler(fh)
    else:
      ch = logging.StreamHandler()
      ch.setLevel(logging.DEBUG)
      ch.setFormatter(formatter)
      logger.addHandler(ch)

    return logger


if __name__ == "__main__":  

  logpath = "/tmp/"
  host = "0.0.0.0"
  port = "12470"
  cont = ""
  opts, args = getopt.getopt(sys.argv[1:], "c:h:L:")
  cfg = os.path.dirname(os.path.realpath(__file__)) + "/paysvr_conf.json"

  for a,opt in opts:
    if (a=='-c'):
      cfg = opt
    elif (a=='-h'):
      host= opt
    elif (a=='-L'):
      logpath= opt

  logger.debug("config file: '{0}'".format(cfg))

  # initialize log
  logger = init_log(logpath)

  # read configs
  with open(cfg,"r") as m_fd:
    cont = m_fd.read()

  # init biz module
  cbbiz = check_bill_biz(cont)
  port = str(cbbiz.get_listen_port())

  # modify web port manually
  sys.argv[1] = (host+":"+port)

  # run application
  ptn = ('/date=(.*)', 'cb_web_req')  
  app = web.application(ptn, globals())  
  app.run()  

