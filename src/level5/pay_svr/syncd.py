#!/usr/bin/python

import pymysql
import redis
import json
import re


class myredis_configs(object):
  address      = "127.0.0.1"
  port         = 6379
  config_table = "myrds_002_cache"
  config_mq    = "myrds_002_mq"
  data_table   = "myrds_001_cache"
  data_mq      = "myrds_001_mq"

class mysql_configs(object):
  address = "127.0.0.1"
  port    = 3306
  db      = "pay_db"
  usr     = "root"
  pwd     = "123"


class myredis_status(object):
  mr__na = 0    # no record
  mr__need_sync = 1  # need Redis -> Mysql
  mr__need_sync_back = 2  # need Mysql -> Redis
  mr__ok = 3  # record is ok


class Mysql(object):

  def __init__(self,host,port,db,usr,pwd):
    self.m_conn = pymysql.connect(host=host,port=port,
                  user=usr,password=pwd,database=db,
                  cursorclass=pymysql.cursors.DictCursor)

  def is_connect(self):
    return self.m_conn.open==True

  def query(self,table,cond=""):
    strsql = ("select *from " + table + " " + cond)

    cursor = self.m_conn.cursor()
    cursor.execute(strsql)

    return cursor.fetchall()
    


class myredis(object):

  def __init__(self,host,port):
    pool = redis.ConnectionPool(host=host,port=port,password='')
    self.m_rds = redis.Redis(connection_pool=pool)

    if (self.m_rds==None):
      print("connect to redis {0}:{1} fail!".format(host,port))
    else:
      print("connect to redis {0}:{1} ok!".format(host,port))


  def read(self,dict1,key):
    if (self.m_rds.hexists(dict1,key)==False):
      return None

    return self.m_rds.hget(dict1,key)
      

  def write(self,dict1,key,val):
    self.m_rds.hset(dict1,key,val)


  def pushq(self,mq,v):
    self.m_rds.rpush(mq,v)

  def popq(self,mq):
    return self.m_rds.lpop(mq)

  def topq(self,mq):
    return self.m_rds.lindex(mq,0)



class syncd(object):


  def __init__(self):

    self.rds_status = myredis_status()
    self.rds_cfg    = myredis_configs()
    self.mysql_cfg  = mysql_configs()
    cfg   = self.rds_cfg
    mscfg = self.mysql_cfg

    self.m_rds   = myredis(cfg.address,cfg.port)
    self.m_mysql = Mysql(mscfg.address,mscfg.port,mscfg.db,
                         mscfg.usr,mscfg.pwd)



  def sync_back_configs(self,key,table,rows):

    rds = self.m_rds
    cfg = self.rds_cfg
    cfgTbl = cfg.config_table
    resMap = {}


    vj = json.dumps(rows)

    resMap['status'] = self.rds_status.mr__ok
    resMap['table']  = table
    resMap['value']  = vj

    fj = json.dumps(resMap)
    print("final res: {0}".format(fj))

    rds.write(cfgTbl,key,fj)



  def do_synchronize(self,rdsTbl,rdsMq):
    rst   = self.rds_status
    rds   = self.m_rds 
    mysql = self.m_mysql


    while (True):

      # read mq for config cache keys
      k = rds.topq(rdsMq)
      if (k==None):
        break

      # process config cache
      v = rds.read(rdsTbl,k)
      if (v==None):
        rds.popq(rdsMq)
        continue

      jres = json.loads(v)
      stat = jres['status']
      tbl  = jres['table']

      if (stat==rst.mr__need_sync_back):
        rows = mysql.query(tbl)
        self.sync_back_configs(k,tbl,rows)

      rds.popq(rdsMq)



  def run(self):
    cfg = self.rds_cfg;

    self.do_synchronize(cfg.config_table,cfg.config_mq)

    self.do_synchronize(cfg.data_table,cfg.data_mq)




def main():

  biz = syncd()

  biz.run()




if __name__=="__main__":
  main()


