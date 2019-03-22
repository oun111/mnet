#!/usr/bin/python

import pymysql
import redis
import json
import re
import time
import sys
import getopt
import decimal


class myredis_configs(object):
  address = "127.0.0.1"
  port    = 6379
  cfgTbl  = "myrds_002"
  cfgMq   = (cfgTbl+"_mq")
  odrTbl = "myrds_003"
  odrMq  = (odrTbl+"_mq")


class mysql_configs(object):
  address = "127.0.0.1"
  port    = 3306
  db      = "pay_db"
  usr     = "root"
  pwd     = "123"


"""
 refer: https://www.cnblogs.com/hanjiajiejie/p/9223511.html
"""
class DecimalEncoder(json.JSONEncoder):

  def default(self, o):

    if isinstance(o, decimal.Decimal):
      return float(o)

    super(DecimalEncoder, self).default(o)



class myredis_status(object):
  mr__na = 0    # no record
  mr__need_sync = 1  # need Redis -> Mysql
  mr__need_sync_back = 2  # need Mysql -> Redis
  mr__ok = 3  # record is ok


class Mysql(object):

  def __init__(self,host,port,db,usr,pwd):
    self.m_conn = pymysql.connect(host=host,port=port,
                  user=usr,password=pwd,database=db,
                  autocommit=False,
                  cursorclass=pymysql.cursors.DictCursor)


  def is_connect(self):
    return self.m_conn.open==True

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
      'merchant_configs'       : ('name', self.sync_back_mch_cfg),
      'channel_alipay_configs' : ('app_id', self.sync_back_alipay_cfg),
      'order_data'             : ('orderid', self.sync_back_order_data)
    }

    self.sync_cb = {
      'merchant_configs'       : ('name',self.sync_mch_cfg),
      'channel_alipay_configs' : ('app_id',self.sync_chan_alipay_cfg),
      'order_data'             : ('orderid',self.sync_order_data)
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
      rcfg.cfgTbl  = cfg['cfgCache']
      rcfg.cfgMq   = (rcfg.cfgTbl + "_mq" )
      rcfg.odrTbl  = cfg['orderCache']
      rcfg.odrMq   = (rcfg.odrTbl + "_mq" )
      print("redis configs: host: {0}:{1}, table: {2}, mq: {3}".
          format(rcfg.address,rcfg.port,rcfg.cfgTbl,rcfg.cfgMq))

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



  def concat_sql_list(self,dct1,t1):

    lst = ''
    first = True

    for i in dct1.keys():
      if (first==False):
        lst = (lst + ",")
      else:
        first = False

      # sql update list
      if (t1==0):
        lst = (lst + str(i) + " = '" + str(dct1[i]) + "'")
      # sql insert list
      elif (t1==1):
        lst = (lst + "`" + str(i) + "`")
      # sql insert value list
      elif (t1==2):
        lst = (lst + "\"" + str(dct1[i]) + "\"")
      else:
        lst = ''

    return lst


  def do_sql_insert_update(self,dct1,table,key):

    print("dct: ",dct1)
    for ch in dct1:
      strSql  = ""
      kval    = ch[key]
      mysql   = self.m_mysql
      strCond = (" where " + key + "='" + kval + "'")
      selist  = "count(1)"

      res = mysql.query(table,selist,strCond)

      if (res[0][selist]>0):

        updList = self.concat_sql_list(ch,0)
        strSql  = ("update "+ table + " set " + 
                   updList +
                   " where " + key  + " = '" + kval + "'")
      else:
        instList = self.concat_sql_list(ch,1)
        valList  = self.concat_sql_list(ch,2)
        strSql   = ("insert into " + table + 
                    "(" + instList + ")" +
                    " values(" + valList + ")")

      print("update sql: "+strSql)

      mysql.update(strSql)


  def sync_order_data(self,table,key,vmap):
    self.do_sql_insert_update(vmap,table,key)


  def sync_chan_alipay_cfg(self,table,key,vmap):
    cm = vmap['channels']
    apmap = cm['alipay']

    self.do_sql_insert_update(apmap,table,key)


  def sync_mch_cfg(self,table,key,vmap):

    mch_map = vmap['merchants']

    self.do_sql_insert_update(mch_map,table,key)


  def do_sync(self,rdsTbl,key,table,v):

    resMap = {}
    rds = self.m_rds

    db_key = self.sync_cb[table][0]
    cb = self.sync_cb[table][1]
    cb(table,db_key,json.loads(v))

    resMap['status'] = self.rds_status.mr__ok
    resMap['table']  = table
    resMap['value']  = v

    fj = json.dumps(resMap)
    rds.write(rdsTbl,key,fj)


  def sync_back_order_data(self,rows):

    top   = {}
    nrows = self.to_lower(rows)

    return json.dumps(nrows,cls=DecimalEncoder),len(nrows)


  def sync_back_mch_cfg(self,rows):

    top   = {}
    nrows = self.to_lower(rows)

    top['merchants'] = nrows

    return json.dumps(top),len(nrows)


  def sync_back_alipay_cfg(self,rows):

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


    # extract target mysql key
    klst   = key.split('#')
    db_key = klst[len(klst)-1]

    # construct sql query condition list
    if (len(db_key)>0):
      k = self.sync_back_cb[table][0]
      cond = " where " + k + " = '" + db_key + "'"
    else:
      cond = ""

    # callback method
    cb = self.sync_back_cb[table][1]

    # query database for results
    rows = mysql.query(table," * ",cond)
    #print("rows: {0}".format(rows))
    vj,numRows = cb(rows)

    if (numRows>0):
      stat = self.rds_status.mr__ok
    else:
      stat = self.rds_status.mr__na

    resMap['status'] = stat
    resMap['table']  = table
    resMap['value']  = vj

    fj = json.dumps(resMap)
    print("final res: {0}".format(fj))

    # sync back to redis
    rds.write(rdsTbl,key,fj)



  def manual_sync_back(self,tbl,key):

    mp1 = {}
    mapList = {
      "order_data"       : (self.rds_cfg.odrTbl,self.rds_cfg.odrMq),
      "merchant_configs" : (self.rds_cfg.cfgTbl,self.rds_cfg.cfgMq),
      "channel_alipay_configs" : (self.rds_cfg.cfgTbl,self.rds_cfg.cfgMq)
    }

    mp1['status'] = self.rds_status.mr__need_sync_back
    mp1['table'] = tbl
    mp1['value'] = "{}"

    rk = (tbl + "#" + key)
    rv = json.dumps(mp1)

    #print("req: "+rv)


    mk = mapList.get(tbl)
    if (mk==None):
      print("table '{0}' not support yet!\n".format(tbl))
      exit(0)

    # commands to syncd
    self.m_rds.write(mk[0],rk,rv)
    self.m_rds.pushq(mk[1],rk)

    print("write syncd command with table '{0}' done\n".format(tbl))



  def do_synchronize(self,rdsTbl,rdsMq):
    rst   = self.rds_status
    rds   = self.m_rds 


    while (True):

      # read mq for cache keys
      k = rds.topq(rdsMq)
      if (k==None):
        break

      # read redis by key
      v = rds.read(rdsTbl,k)
      if (v==None):
        rds.popq(rdsMq)
        continue

      jres = json.loads(v)
      stat = jres['status']
      tbl  = jres['table']
      v    = jres['value']

      # sync:  mysql -> redis
      if (stat==rst.mr__need_sync_back):
        self.do_sync_back(rdsTbl,str(k),tbl)
      # sync: redis -> mysql
      elif (stat==rst.mr__need_sync):
        self.do_sync(rdsTbl,str(k),tbl,v)

      rds.popq(rdsMq)



  def run(self):
    cfg = self.rds_cfg;

    while (True):

      self.do_synchronize(cfg.cfgTbl,cfg.cfgMq)

      self.do_synchronize(cfg.odrTbl,cfg.odrMq)

      time.sleep(0.08);



def start_sync_svr(biz):

  print("starting local synchronize server...")

  biz.run()


def manual_mysql_2_rds(biz,tbl,key):
  
  biz.manual_sync_back(tbl,key)


def manual_rds_2_mysql(biz,tbl,key,val):
  pass


def main():

  tbl = ""
  key = ""
  val = ""
  cfg = ""
  cont = ""
  opts, args = getopt.getopt(sys.argv[1:], "hc:t:k:v:")

  for a,opt in opts:
    if (a=='-c'):
      cfg = opt
    elif (a=='-t'):
      tbl = opt
    elif (a=='-k'):
      key = opt
    elif (a=='-v'):
      val = opt


  if (len(cfg)==0):
    print("need config file path...")
    exit(0)


  # read configs
  with open(cfg,"r") as m_fd:
    cont = m_fd.read()

  # initialize the interfaces
  biz = syncd(cont)


  # run the syncd server
  if (len(tbl)==0 and len(key)==0 and len(val)==0):
    start_sync_svr(biz)
    exit(0)

  # write a command to request: mysql -> redis
  if (len(val)==0):
    manual_mysql_2_rds(biz,tbl,key)
  else:
    manual_rds_2_mysql(biz,tbl,key,val)



if __name__=="__main__":
  main()


