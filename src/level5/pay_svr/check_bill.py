#!/usr/bin/python


import web  
import json
import pymysql
import re
import os
import time
import datetime
import sys
import getopt
import decimal
import sys
from Crypto.PublicKey import RSA
from Crypto.Signature import PKCS1_v1_5
from Crypto.Hash import SHA256
from base64 import b64encode,b64decode
#from base64 import decodebytes,encodebytes
import urllib2




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


  def do_biz(self,bill_date):
    rows = self.m_mysql.query(self.mysql_cfg.chanTbl," * "," where online='1' ")

    for r in rows:
      pay_params  = {}
      biz_content = {}
      pay_params['app_id'] = r['APP_ID']
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
      sort_str  = "&".join("{0}={1}".format(k, urllib2.quote(v)) for k, v in sort_list)
      reqstr = (r['REQ_URL'] + "?" + sort_str)

      #print("final request string: {0}".format(reqstr))

      res = urllib2.urlopen(reqstr)

      print(res)


  def sign(self, privkey, signtmp):
    signer = PKCS1_v1_5.new(privkey)
    signature = signer.sign(SHA256.new(signtmp))
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

