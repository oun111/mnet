#!/usr/bin/python

import pymysql
import redis
import json


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

  def __init__(object,host,port,db,usr,pwd):
    self.m_conn = pymysql.connect(host=host,port=port,
                  user=usr,password=pwd,database=db)


  def query(self,table,key=None):
    strsql = ("select *from " + table)

    self.m_conn.execute(strsql)
    


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
    self.rds_cfg = myredis_configs()
    self.mysql_cfg = mysql_configs()
    cfg = self.rds_cfg
    mscfg = self.mysql_cfg

    self.m_rds = myredis(cfg.address,cfg.port)

    self.m_mysql = Mysql(mscfg.address,mscfg.port,mscfg.db,
                         mscfg.usr,mscfg.pwd)


  def synchronize_configs(self):
    rst  = self.rds_status
    cfg = self.rds_cfg
    mscfg = self.mysql_cfg
    rds = self.m_rds 
    mysql = self.m_mysql
    cfgTbl = cfg.config_table
    cfgMq  = cfg.config_mq



    while (True):

      # read mq for config cache keys
      k = rds.topq(cfgMq)
      if (k==None):
        break

      # process config cache
      v = rds.read(cfgTbl,k)

      jres = json.loads(v)
      stat = jres['status']
      tbl  = jres['table']

      if (stat==rst.mr__need_sync_back):
        print("need to sync from mysql, table: {0}!!".format(tbl))

        mysql.query(tbl)

      rds.popq(cfgMq)


  def synchronize_data(self):
    # TODO
    pass


  def run(self):
    self.synchronize_configs()

    self.synchronize_data()




def main():

  biz = syncd()

  biz.run()




if __name__=="__main__":
  main()


