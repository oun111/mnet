#!/usr/bin/python

import pymysql
import redis
import json
import re
import time
import sys
import getopt


class myredis_configs(object):
  address = "127.0.0.1"
  port    = 6379
  table   = "myrds_002"
  mq      = (table+"_mq")


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

  def query(self,table,selist=" * ",cond=""):
    strsql = ("select " + selist + " from " + table + " " + cond)

    cursor = self.m_conn.cursor()
    cursor.execute(strsql)

    return cursor.fetchall()


  def update(self,strSql):
    cursor = self.m_conn.cursor()

    cursor.execute(strSql)
    self.m_conn.commit()
    


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


  def __init__(self,cfg_contents):

    self.sync_back_cb = {
      'merchant_configs'       : self.conv_mch_cfg_formats,
      'channel_alipay_configs' : self.conv_alipay_cfg_formats
    }

    self.sync_cb = {
      'merchant_configs'       : self.sync_mch_cfg,
      'channel_alipay_configs' : self.sync_chan_alipay_cfg
    }

    self.rds_status = myredis_status()
    self.rds_cfg    = myredis_configs()
    self.mysql_cfg  = mysql_configs()
    rcfg  = self.rds_cfg
    mscfg = self.mysql_cfg

    # update configs
    gcfg = json.loads(cfg_contents)
    cfg = gcfg['Globals']['Redis']

    if (len(cfg)>0):
      rcfg.address = cfg['address']
      rcfg.port    = cfg['port']
      rcfg.table   = cfg['cfgCache']
      rcfg.mq      = (cfg['cfgCache'] + "_mq" )
      print("redis configs: host: {0}:{1}, table: {2}, mq: {3}".
          format(rcfg.address,rcfg.port,rcfg.table,rcfg.mq))

    self.m_rds   = myredis(rcfg.address,rcfg.port)
    self.m_mysql = Mysql(mscfg.address,mscfg.port,mscfg.db,
                         mscfg.usr,mscfg.pwd)



  def to_lower(self,rows):
    nrows = []

    for r in rows:
      newr = {}
      for k in r:
        newr[k.lower()] = r[k]
      nrows.append(newr)

    return nrows


  def conv_mch_cfg_formats(self,rows):

    top   = {}
    nrows = self.to_lower(rows)

    top['merchants'] = nrows

    return json.dumps(top),len(nrows)


  def conv_alipay_cfg_formats(self,rows):

    top = {}
    alip= {}
    nrows = self.to_lower(rows)

    alip['alipay'] = nrows
    top['channels']= alip

    return json.dumps(top),len(nrows)


  def do_sync_back(self,rdsTbl,key,table):

    mysql = self.m_mysql
    rds   = self.m_rds
    resMap = {}

    rows = mysql.query(table)
    vj,numRows = self.sync_back_cb[table](rows)

    if (numRows>0):
      stat = self.rds_status.mr__ok
    else:
      stat = self.rds_status.mr__na

    resMap['status'] = stat
    resMap['table']  = table
    resMap['value']  = vj

    fj = json.dumps(resMap)
    print("final res: {0}".format(fj))

    rds.write(rdsTbl,key,fj)



  def sync_mch_cfg(self,table,vmap):

    mch_map = vmap['merchants']

    for mch in mch_map:
      strSql  = ""
      mysql   = self.m_mysql
      name    = mch['name']
      selist  = "count(1)"
      strCond = (" where name='" + name + "'")

      res = mysql.query(table,selist,strCond)

      if (res[0][selist]>0):
        strSql = ("update "+ table + " set sign_type='" + mch['sign_type'] + "'," +
                  " param_type = '" + mch['param_type'] + "'," + 
                  " pubkey = '"  + mch['pubkey'] + "'," + 
                  " privkey = '" + mch['privkey'] + "' "+ 
                  " where name = '" + name + "'")
      else:
        strSql = ("insert into " + table + "(name,sign_type,pubkey,privkey,param_type)" +
                  " values(" 
                  "'" + name + "', '" + mch['sign_type'] + "', " 
                  "'" + mch['pubkey'] + "', '" + mch['privkey'] + "' ," 
                  "'" + mch['param_type'] + "' )" )

      #print("update sql: "+strSql)

      mysql.update(strSql)



  def sync_chan_alipay_cfg(self,table,vmap):
    cm = vmap['channels']
    apmap = cm['alipay']

    for ch in apmap:
      strSql  = ""
      app_id  = ch['app_id']
      mysql   = self.m_mysql
      strCond = (" where app_id='" + app_id + "'")
      selist  = "count(1)"

      res = mysql.query(table,selist,strCond)

      if (res[0][selist]>0):
        strSql = ("update "+ table + " set "
                  " name ='" + ch['name'] + "'," +
                  " req_url = '" + ch['req_url'] + "'," + 
                  " param_type = '" + ch['param_type'] + "'," + 
                  " public_key_path = '"  + ch['public_key_path'] + "', " + 
                  " private_key_path = '" + ch['private_key_path'] + "', " + 
                  " TIMEOUT_EXPRESS = '"  + ch['timeout_express'] + "', " + 
                  " product_code = '" + ch['product_code'] + "', " + 
                  " METHOD = '"  + ch['method'] + "', " + 
                  " FORMAT = '"  + ch['format'] + "', " + 
                  " CHARSET = '" + ch['charset'] + "', " + 
                  " VERSION = '" + ch['version'] + "', " + 
                  " SIGN_TYPE = '"  + ch['sign_type'] + "', " + 
                  " NOTIFY_URL = '" + ch['notify_url'] + "', " + 
                  " RETURN_URL = '" + ch['return_url'] + "' " + 
                  " where app_id = '" + app_id          + "'")
      else:
        strSql = ("insert into " + table + 
                  " (NAME,REQ_URL,PARAM_TYPE,PUBLIC_KEY_PATH,PRIVATE_KEY_PATH,PRODUCT_CODE," +
                  "  TIMEOUT_EXPRESS,APP_ID,METHOD,FORMAT,CHARSET,SIGN_TYPE,VERSION,NOTIFY_URL," +
                  "  RETURN_URL)" +
                  " values(" + 
                  "'" + ch['name'] + "', '" + ch['req_url'] + "', '" + ch['param_type'] + "', "
                  "'" + ch['public_key_path'] + "', '" + ch['private_key_path'] + "', "
                  "'" + ch['product_code'] + "', '" + ch['timeout_express'] + "', "  
                  "'" + ch['app_id'] + "', '" + ch['method'] + "', '" + ch['format'] + "', "
                  "'" + ch['charset'] + "', '" + ch['version'] + "', '" + ch['sign_type'] + "', "
                  "'" + ch['notify_url'] + "', '" + ch['return_url'] + "' )" )

      #print("update sql: "+strSql)

      mysql.update(strSql)



  def do_sync(self,rdsTbl,key,table,v):

    resMap = {}
    rds = self.m_rds

    self.sync_cb[table](table,json.loads(v))

    resMap['status'] = self.rds_status.mr__ok
    resMap['table']  = table
    resMap['value']  = v

    fj = json.dumps(resMap)
    rds.write(rdsTbl,key,fj)



  def do_synchronize(self,rdsTbl,rdsMq):
    rst   = self.rds_status
    rds   = self.m_rds 


    while (True):

      # read mq for cache keys
      k = rds.topq(rdsMq)
      if (k==None):
        break

      # process cache
      v = rds.read(rdsTbl,k)
      if (v==None):
        rds.popq(rdsMq)
        continue

      jres = json.loads(v)
      stat = jres['status']
      tbl  = jres['table']
      v    = jres['value']

      if (stat==rst.mr__need_sync_back):
        self.do_sync_back(rdsTbl,k,tbl)
      elif (stat==rst.mr__need_sync):
        self.do_sync(rdsTbl,k,tbl,v)

      rds.popq(rdsMq)



  def run(self):
    cfg = self.rds_cfg;

    while (True):

      self.do_synchronize(cfg.table,cfg.mq)

      time.sleep(0.1);



def main():

  conf_path  = ""
  cont = ""
  opts, args = getopt.getopt(sys.argv[1:], "hc:")

  for a,opt in opts:
    if (a=='-c'):
      conf_path = opt
      
  with open(conf_path,"r") as m_fd:
    cont = m_fd.read()


  biz = syncd(cont)

  biz.run()




if __name__=="__main__":
  main()


