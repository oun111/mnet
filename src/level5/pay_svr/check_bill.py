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




class mysql_configs(object):
  address = "127.0.0.1"
  port    = 3306
  db      = ""
  usr     = ""
  pwd     = ""
  odrTbl  = ""
  chanTbl = ""


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
    #print("sql: " + strsql)

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
      mscfg.chanTbl = cfg['alipayConfigTableName']
      print("mysql configs: host: {0}:{1}".
            format(mscfg.address,mscfg.port))

    self.m_mysql = Mysql(mscfg.address,mscfg.port,mscfg.db,
                         mscfg.usr,mscfg.pwd)

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

    #print("final request string: {0}".format(reqstr))

    req = urllib.request.Request(reqstr)
    res = urllib.request.urlopen(req)
    res = res.read()

    pr = json.loads(res)
    lnk= pr['alipay_data_dataservice_bill_downloadurl_query_response'].get('bill_download_url')

    if lnk==None:
      reason = pr['alipay_data_dataservice_bill_downloadurl_query_response']['sub_msg']
      print("get download link by app_id '{0}' bill-date '{1}' fail: {2}"
            .format(app_id,bill_date,reason))

    return lnk 



  def download_check_file(self,appid,basepath,bd,lnk):
    #print("trying link: " + lnk)

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
      #print("xf: " + xf.encode(encoding=u'cp437',errors='ignore').decode(u'gb2312'))
      xf_info = zf.getinfo(xf)
      xf_path = (basepath+"/"+appid+"_"+(xf.encode(encoding=u'cp437',errors='ignore').decode(u'gb2312')))

      with open(xf_path,"wb") as f:
        xff = zf.open(xf_info)
        f.write(xff.read())

    os.remove(zfpath)

    
  def do_summerize_db(self,appid,bd):
    tstruct = time.strptime(bd,"%Y-%m-%d")
    if tstruct==None:
      print("invalid bill date: "+bd)
      return 

    prev_date = (time.mktime(tstruct)+2208988800)*1000000
    next_date = prev_date + 86400*1000000

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

        #print("parsing csv " + f)

        with codecs.open(basepath+"/"+f,'r','gb2312') as fcsv:
          fc = csv.reader(fcsv)
          for r in fc:
            if r[0]=='合计\t':
              return float(r[5])

    return None



  def do_biz(self,bill_date):
    rows = self.m_mysql.query(self.mysql_cfg.chanTbl," * "," where /*online*/1=1 ")

    basepath = "/tmp/" + "alipay_bills/" + bill_date + "/"

    for r in rows:
      lnk  = self.get_download_link(r,bill_date)
      appid= r['APP_ID']

      if lnk==None:
        continue

      self.download_check_file(appid,basepath,bill_date,lnk)

      db_sum = self.do_summerize_db(appid,bill_date)

      bill_sum = self.do_summerize_check_file(basepath,appid,bill_date)

      print("appid '{0}' db sum {1} bill sum {2}".format(appid,db_sum,bill_sum))

      if bill_sum!=None and db_sum!=None and bill_sum==db_sum:
        print("appid '{0}' sum {1} in db and bill are equal!".format(appid,bill_sum))



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
  

if __name__ == "__main__":  

  host = "0.0.0.0"
  port = "12470"
  cont = ""
  opts, args = getopt.getopt(sys.argv[1:], "c:p:h:")
  cfg = os.path.dirname(os.path.realpath(__file__)) + "/paysvr_conf.json"

  for a,opt in opts:
    if (a=='-c'):
      cfg = opt
    elif (a=='-p'):
      port= opt
    elif (a=='-h'):
      host= opt

  print("config file: '{0}'".format(cfg))

  # read configs
  with open(cfg,"r") as m_fd:
    cont = m_fd.read()

  # init biz module
  cbbiz = check_bill_biz(cont)

  # modify port manually
  sys.argv[1] = (host+":"+port)

  # run application
  ptn = ('/(.*)', 'cb_web_req')  
  app = web.application(ptn, globals())  
  app.run()  

