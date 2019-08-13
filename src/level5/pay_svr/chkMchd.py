#!/usr/bin/python3

import json
import pymysql
import re
import os
import time
import datetime
import getopt
import sys
import urllib
import chardet
import logging
import logging.handlers


appname = "chkMchd"
logger = logging.getLogger((appname + '_log'))


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

    logger.debug("sql: " + sql)

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
  def microSecs_by_offset(offs=None):

    tstr = datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')
    tstruct = time.strptime(tstr,"%Y-%m-%d %H:%M:%S")
    if tstruct==None:
      logger.debug("invalid time ")
      return -1

    ms = 0
    if offs!=None:
      ms = offs * 1000000

    return (time.mktime(tstruct)+2208988800)*1000000 - ms

  @staticmethod
  def pyList_to_myList(lst):
    res = ",".join(lst)
    if len(res)==0:
      res = "''"
    return res 


class check_mch_biz:
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
      logger.debug("mysql configs: host: {0}:{1}".
            format(mscfg.address,mscfg.port))

    self.m_mysql = Mysql(mscfg.address,mscfg.port,mscfg.db,
                         mscfg.usr,mscfg.pwd)


  def do_biz(self,period):

    if period==-1:
      updateSql = "update {chanTbl} set status=0 ".format(chanTbl=self.mysql_cfg.chanTbl)
      self.m_mysql.update(updateSql)
      return


    rows = self.m_mysql.query(self.mysql_cfg.chanTbl," * "," where 1=1")

    times = simple_tools.microSecs_by_offset(offs=period)

    for r in rows:
      appid = r['APP_ID']
      strsql= "select unpay.count unpay_count, total.count total_count from (select count(1) count from {odrTbl} where CHAN_MCH_NO='{mch_no}' and status<>2 and create_time>={ts}  ) unpay, (select count(1) count from {odrTbl} where CHAN_MCH_NO='{mch_no}' and create_time>={ts}  ) total where 1=1".format(mch_no=appid,ts=times,odrTbl=self.mysql_cfg.odrTbl)

      res = self.m_mysql.do_exec(strsql)
      total = res[0]['total_count']
      unpay = res[0]['unpay_count']

      stat = 0
      # 该通道未使用
      if total==0:
        stat = 0
      # 该通道被封
      elif total==unpay:
        stat = 2
      # 该通道正常
      else:
        stat = 1

      updateSql = "update {chanTbl} set status={stat} where app_id='{app_id}'"\
                  .format(chanTbl=self.mysql_cfg.chanTbl,stat=stat,app_id=appid)
      self.m_mysql.update(updateSql)
      #print("id: {0}, appid: {1}, stat: {2}".format(r['id'],appid,stat))


  def getPeriodByDatetime(self):

    t = time.time()
    tstruct = time.localtime(t)

    # 周六、日 不探测
    if tstruct.tm_wday==0 or tstruct.tm_wday==6:
      return -1

    # 3 ~6 不探测
    if tstruct.tm_hour>=3 and tstruct.tm_hour<=6:
      return -1

    # 8~12，14~24, 0~1 周期为5分钟
    if tstruct.tm_hour<=1 or tstruct.tm_hour>=14 or (tstruct.tm_hour>=8 and tstruct.tm_hour<=12):
      return 5*60

    # 其余周期为 15 分钟
    return 15*60


  def run(self):

    try:

      while (True):

        period = self.getPeriodByDatetime()

        self.do_biz(period)

        time.sleep(5);

    except:
      logger.exception("logging exception")


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
  cont = ""
  opts, args = getopt.getopt(sys.argv[1:], "c:L:")
  cfg = os.path.dirname(os.path.realpath(__file__)) + "/paysvr_conf.json"

  for a,opt in opts:
    if (a=='-c'):
      cfg = opt
    elif (a=='-L'):
      logpath= opt

  logger.debug("config file: '{0}'".format(cfg))

  # initialize log
  logger = init_log(logpath)

  # read configs
  with open(cfg,"r") as m_fd:
    cont = m_fd.read()

  # init biz module
  cbbiz = check_mch_biz(cont)
  cbbiz.run()


