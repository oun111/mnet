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


logger = logging.getLogger('cbd_log')


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


class Mysql(object):
  def __init__(self,host,port,db,usr,pwd):
    self.do_connect(host,port,db,usr,pwd)


  def do_connect(self,host,port,db,usr,pwd):
    self.m_conn = pymysql.connect(host=host,port=port,
                  user=usr,password=pwd,database=db,
                  autocommit=False,
                  cursorclass=pymysql.cursors.DictCursor)


  def query(self,table,selist=" * ",cond=""):
    strsql = ("select " + selist + " from " + table + " " + cond)
    #logger.debug("sql: " + strsql)

    self.m_conn.commit()

    with self.m_conn.cursor() as cs:
      cs.execute(strsql)

      res = cs.fetchall()

    self.m_conn.commit()

    return res


  def update(self,strSql):

    with self.m_conn.cursor() as cs:

      cs.execute(strSql)
      self.m_conn.commit()
    

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

    biz_content['bill_type']= 'trade'
    biz_content['bill_date']= bill_date

    pay_params['biz_content'] = json.dumps(biz_content)

    # building pre sign string
    sort_list = sorted(pay_params.items())
    sort_str  = "&".join("{0}={1}".format(k, v) for k, v in sort_list)

    privkey = ""
    with open(r['PRIVATE_KEY_PATH']) as fp:
      privkey = RSA.importKey(fp.read())

    pay_params['sign'] = self.sign(privkey,sort_str)
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


  def dateStr_to_microSecs(self,date):

    tstruct = time.strptime(date,"%Y-%m-%d")
    if tstruct==None:
      logger.debug("invalid date: "+date)
      return -1

    return (time.mktime(tstruct)+2208988800)*1000000


  def get_nextDate_microSecs(self,ds):
    return (ds + 86400*1000000)

    
  def do_summerize_db(self,appid,bd):

    prev_date = self.dateStr_to_microSecs(bd)
    next_date = self.get_nextDate_microSecs(prev_date)

    result = self.m_mysql.query(self.mysql_cfg.odrTbl," sum(ifnull(amount,0)) amt ",
                                " where chan_mch_no='{0}' and create_time>={1} and " \
                                " create_time<={2} and status=2 "
                                .format(appid,prev_date,next_date))

    rs = result[0].get('amt')

    if rs!=None:
      return float(rs)

    return None


    
  def do_summerize_check_file(self,basepath,appid,bd):

    for __path,__subPaths,__files in os.walk(basepath):

      #scanning file under __path
      for f in __files:

        if bool(re.match((appid+"_.*\(汇总\).csv"),f))!=True :
          continue

        #logger.debug("parsing csv " + f)

        with codecs.open(basepath+"/"+f,'r','gb18030') as fcsv:
          fc = csv.reader(fcsv)
          for r in fc:
            if r[0]=='合计\t':
              return float(r[5])

    return None


  def check_summerize(self,basepath,appid,bill_date):

    logger.debug("checking appid '{0}' summerize...".format(appid))

    db_sum = self.do_summerize_db(appid,bill_date)

    bill_sum = self.do_summerize_check_file(basepath,appid,bill_date)

    #logger.debug("appid '{0}' db sum {1} bill sum {2}".format(appid,db_sum,bill_sum))

    if bill_sum==None or db_sum==None or bill_sum!=db_sum:
      logger.debug("appid '{0}' db sum {1} NOT equal with bill sum {2}!!".format(appid,bill_sum,db_sum))



  def get_db_order_details(self,bill_date):

    db_dict = {}
    chanlist= ""

    rows = self.m_mysql.query(self.mysql_cfg.chanTbl," * "," where istransfund=0 ")

    for i in range(len(rows)):
      if i>0:
        chanlist = chanlist + ','
      chanlist = chanlist + '\'' + rows[i]['APP_ID'] + '\''


    prev_date = self.dateStr_to_microSecs(bill_date)
    next_date = self.get_nextDate_microSecs(prev_date)

    # check db details by mch names
    odr_rows = self.m_mysql.query(self.mysql_cfg.odrTbl," * ",
                              " where create_time>={0} and create_time<={1} and "\
                              " status=2 and chan_mch_no in({2})".format(prev_date,next_date,chanlist))

    for odr in odr_rows:
      db_dict[odr['MCH_ORDERID']] = (float(odr['AMOUNT']),odr['MCH_NO'],odr['CHAN_MCH_NO'])


    return db_dict

      

  def get_check_file_details(self,basepath):

    cf_dict = {}

    for __path,__subPaths,__files in os.walk(basepath):

      for f in __files:

        if bool(re.match((".*明细.csv"),f))!=True :
          continue

        #logger.debug("parsing csv " + f)

        with codecs.open(basepath+"/"+f,'r','gb18030') as fcsv:
          fc = csv.reader(fcsv)

          for r in fc:

            if len(r[0])==0 or r[0][0]=='#' or r[0][0:6].isdigit()==False:
              continue

            # skip '\t' at last position
            cf_dict[r[1][:-1]] = (float(r[12]),r[3])

    return cf_dict


  def errStr(self,err):
    if err==1:
      return '金额不等'
    elif err==2:
      return '账单缺流水'
    elif err==3:
      return '数据库缺流水'
    return '未知错误类型: {0}'.format(err)


  def save_unpair_record(self,k,v,e):
    self.m_mysql.update(" insert into {0} (MCH_ORDERID,ERROR_TYPE,ERROR_DESC,MCH_DESC) values({1},{2},{3},{4},{5})".format('tbl','ab-11',1,'aa','bb')
#.format(self.mysql_cfg.cbTbl,k,e,self.errStr(e),v[1])
                       )


  def check_details(self,bill_date,basepath):

    db_dict = self.get_db_order_details(bill_date)
    #logger.debug("orders: {0}".format(db_dict))

    cf_dict = self.get_check_file_details(basepath)
    #logger.debug("orders: {0}".format(cf_dict))

    db_dict_copy = db_dict.copy()

    for ckey in cf_dict.keys():
      db_ret = db_dict.get(ckey)

      if db_ret==None:
        continue

      amt = db_ret[0]

      if amt!=cf_dict[ckey][0]:
        self.save_unpair_record(ckey,cf_dict[ckey],1)
        logger.debug("error1: amount no equal: {0}".format(ckey))
        continue

      db_dict.pop(ckey)


    for dkey in db_dict_copy.keys():
      if cf_dict.get(dkey)!=None:
        cf_dict.pop(dkey)


    for k in db_dict.keys():
      self.save_unpair_record(k,db_dict[k],2)
      logger.debug("error2: check none, db has: {0}".format(k))


    for k in cf_dict.keys():
      print("cfd: {0}".format(cf_dict[k]))
      self.save_unpair_record(k,cf_dict[k],3)
      logger.debug("error3: check has, db none: {0}".format(k))

    logger.debug("details checking done!")



  def do_biz(self,bill_date):
    rows = self.m_mysql.query(self.mysql_cfg.chanTbl," * "," where /*online*/1=1 ")

    basepath = self.cb_path + "alipay_bills/" + bill_date + "/"


    """
    for r in rows:
      lnk  = self.get_download_link(r,bill_date)
      appid= r['APP_ID']

      if lnk==None:
        continue

      self.download_check_file(appid,basepath,bill_date,lnk)

      # check summerize by appid one by one
      self.check_summerize(basepath,appid,bill_date)
    """

    # check order details
    self.check_details(bill_date,basepath)


  def sign(self, privkey, signtmp):
    signer = PKCS1_v1_5.new(privkey)
    signature = signer.sign(SHA256.new(signtmp.encode('utf-8')))
    #sign = encodebytes(signature).decode("utf8").replace("\n", "")
    sign = b64encode(signature).decode("utf8").replace("\n", "")
    return sign

  
class cb_web_req:  
  def GET(self, bill_date):  
    cbbiz.do_biz(bill_date)
    return 'check bill on date {0} success!'.format(bill_date)  
  


def init_log(logpath):
    logger.setLevel(logging.DEBUG)

    #fh = logging.FileHandler("/tmp/cbd.log")

    formatter = logging.Formatter('%(asctime)s - %(module)s(%(process)d)%(funcName)s.%(lineno)d - %(levelname)s - %(message)s')

    if (len(logpath)>0):
      fh = logging.handlers.TimedRotatingFileHandler((logpath+"/"+"cbd.log"),when='D')
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

