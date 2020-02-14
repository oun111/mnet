#!/usr/bin/python3
import pymysql
import json
import re
import os
import time
import sys
import getopt
import decimal
import logging
import logging.handlers
import sys
import requests
import urllib.request
from urllib import parse



logger = logging.getLogger('rcd_log')


class mysql_configs(object):
  address = "127.0.0.1"
  port    = 3306
  rctbl   = ""
  mchtbl  = ""
  db      = ""
  usr     = ""
  pwd     = ""


"""
 refer: https://www.cnblogs.com/hanjiajiejie/p/9223511.html
"""
class DecimalEncoder(json.JSONEncoder):

  def default(self, o):

    if isinstance(o, decimal.Decimal):
      return float(o)

    super(DecimalEncoder, self).default(o)



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

    """logger.debug("sql: " + sql)"""

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
    

class rcd(object):

  def __init__(self,cfg_contents):

    self.mysql_cfg  = mysql_configs()
    mscfg = self.mysql_cfg


    #
    # update mysql configs
    # 
    gcfg = json.loads(cfg_contents)
    cfg = gcfg['Globals']['Mysql']
    if (len(cfg)>0):
      mscfg.address = cfg['address']
      mscfg.port    = cfg['port']
      mscfg.db      = cfg['db']
      mscfg.usr     = cfg['usr']
      mscfg.pwd     = cfg['pwd']
      mscfg.rctbl   = cfg['miscRcTableName']
      mscfg.mchtbl  = cfg['merchantConfigTableName']
      logger.debug("mysql configs: host: {0}:{1}".
          format(mscfg.address,mscfg.port))

    self.m_mysql = Mysql(mscfg.address,mscfg.port,mscfg.db,
                         mscfg.usr,mscfg.pwd)

    self.svc_port= gcfg['Globals']['ListenPort']



  def to_lower(self,rows):
    nrows = []

    for r in rows:
      newr = {}
      for k in r:
        newr[k.lower()] = r[k]
      nrows.append(newr)

    return nrows


  """清空 支付服务的 风控缓存"""
  def reset_rc_proc(self):
    # 默认清理时间：每天5：00
    hh = 5
    mm = 0
    ss = 0
    mchid = 'mch_xxx'

    # 查询配置的清空时间
    table= self.mysql_cfg.rctbl
    rows = self.m_mysql.query(table," reset_time ")
    if len(rows) > 0:
      rows = self.to_lower(rows)
      osec = rows[0]['reset_time']
      hh   = int(osec/60/60)
      mm   = int(osec/60%60)
      ss   = int(osec%60)

    # 随便取一个商户名，用于payd接入鉴权
    table = self.mysql_cfg.mchtbl
    rows = self.m_mysql.query(table," name ", " limit 1")
    if len(rows)>0:
      rows = self.to_lower(rows)
      mchid= rows[0]['name']
    
    ts = time.time()
    tm = time.localtime(ts)

    if tm.tm_hour==hh and tm.tm_min==mm and tm.tm_sec==ss:
      url = 'http://127.0.0.1:' + str(self.svc_port) + '/alipay/reset-rc'
      data= {'mch_id':mchid }

      data= parse.urlencode(data).encode('utf-8')

      req = urllib.request.Request(url,data=data)
      res = urllib.request.urlopen(req).read()
      res = str(res).replace('b','').replace('\'','')
      #print("response：{0}".format(res))

      if res == 'success':
        logger.debug("reset rc ok!!")
      else:
        logger.error("reset rc fail: {0}".format(res))



  def run(self):

    while (True):

      try:
        self.reset_rc_proc()

        time.sleep(1)


      except(Exception):
        logger.exception("logging exception")



def start_rc_svr(biz):

  biz.run()


def init_log(logpath):
    logger.setLevel(logging.DEBUG)

    #fh = logging.FileHandler("/tmp/rcd.log")

    formatter = logging.Formatter('%(asctime)s - %(module)s(%(process)d)%(funcName)s.%(lineno)d - %(levelname)s - %(message)s')

    if (len(logpath)>0):
      fh = logging.handlers.TimedRotatingFileHandler((logpath+"/"+"rcd.log"),when='D')
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


def main():

  tbl = ""
  cont = ""
  logpath = ""
  opts, args = getopt.getopt(sys.argv[1:], "hc:L:")
  cfg = os.path.dirname(os.path.realpath(__file__)) + "/paysvr_conf.json"

  print("default config file: '{0}'".format(cfg))

  for a,opt in opts:
    if (a=='-c'):
      cfg = opt
    elif (a=='-L'):
      logpath = opt


  # initialize log
  logger = init_log(logpath)

  # read configs
  with open(cfg,"r") as m_fd:
    cont = m_fd.read()

  # start
  biz = rcd(cont)
  start_rc_svr(biz)


if __name__=="__main__":
  main()


