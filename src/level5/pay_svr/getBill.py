#!/usr/bin/python3 -B

import os
import sys
import json
import redis
import getopt
import time
import random
import logging
import logging.handlers
from selenium import webdriver
from selenium.common.exceptions import NoSuchElementException
from selenium.common.exceptions import TimeoutException
import selenium.webdriver.support.ui as ui
import requests
import urllib.request
from urllib import parse
import traceback


globalName = 'billCollector'
logger     = logging.getLogger(globalName + '_log')



class myredis(object):

  def __init__(self,host,port,succCache,failCache):
    self.host   = host
    self.port   = port
    self.succCache= succCache
    self.failCache= failCache
    self.do_connect()

    if (self.m_rds==None):
      logger.debug("connect to redis {0}:{1} fail!".format(host,port))
    else:
      logger.debug("connect to redis {0}:{1} ok!".format(host,port))

  def do_connect(self):
    pool = redis.ConnectionPool(host=self.host,port=self.port,password='')
    self.m_rds = redis.Redis(connection_pool=pool)

  def read(self,dict1,key):
    while True:
      try:
        if (self.m_rds.hexists(dict1,key)==False):
          return None
        return self.m_rds.hget(dict1,key)
      except redis.ConnectionError as e:
        self.do_connect()

  def write(self,dict1,key,val):
    while True:
      try:
        self.m_rds.hset(dict1,key,val)
        break
      except redis.ConnectionError as e:
        self.do_connect()

  def delete(self,dict1,key=None):
    while True:
      try:
        self.m_rds.delete(dict1)
        break
      except redis.ConnectionError as e:
        self.do_connect()

  def pushq(self,mq,v):
    while True:
      try:
        self.m_rds.rpush(mq,v)
        break
      except redis.ConnectionError as e:
        self.do_connect()

  def publish(self,mq,v):
    while True:
      try:
        self.m_rds.publish(mq,v)
        break
      except redis.ConnectionError as e:
        self.do_connect()

  def popq(self,mq):
    while True:
      try:
        return self.m_rds.lpop(mq)
      except redis.ConnectionError as e:
        self.do_connect()

  def topq(self,mq):
    while True:
      try:
        return self.m_rds.lindex(mq,0)
      except redis.ConnectionError as e:
        self.do_connect()

  def save(self):
    while True:
      try:
        self.m_rds.save()
      except redis.ConnectionError as e:
        self.do_connect()


 
class billCollector(object):
 
    def __init__(self,cfgContent,user='', passwd=''):

        self.parse_configs(cfgContent)
        self.user      = user
        self.passwd    = passwd
        self.loginName = ''
        self.init_driver()
        """
        self.loginName = 'hzcx2019_01@163.com'
        """
         

    def parse_configs(self,cont):
        cfg = json.loads(cont)

        k = 'notifyUrl'
        c = cfg.get(k)
        if (c==None or len(c)==0):
            logger.error("Configure ERROR: '{0}'".format(k))
            exit(-1)
        self.notifyUrl = c

        k = 'enableChanUrl'
        c = cfg.get(k)
        if (c==None or len(c)==0):
            logger.error("Configure ERROR: '{0}'".format(k))
            exit(-1)
        self.enableChanUrl = c

        k = 'safeDelayRange'
        c = cfg.get(k)
        if (c==None or len(c)==0):
            logger.error("Configure ERROR: '{0}'".format(k))
            exit(-1)
        rl= c.split(',')
        if len(rl)!=2:
            logger.error("Configure ERROR1: '{0}'".format(k))
            exit(-1)

        self.minDelay = int(rl[0])
        self.maxDelay = int(rl[1])

        k = 'otnKeyword'
        c = cfg.get(k)
        if (c==None or len(c)==0):
            logger.error("Configure ERROR: '{0}'".format(k))
            exit(-1)
        self.otnKeyword = c

        logger.info("Notify URL: " + self.notifyUrl)
        logger.info("Safety Delay Time Range: ({0},{1})".format(self.minDelay,self.maxDelay))
        logger.info("Order Identity Mark: " + self.otnKeyword)

        k = 'antiPaPages'
        c = cfg.get(k)
        if (c==None or len(c)==0):
            logger.error("Configure ERROR: '{0}'".format(k))
            exit(-1)
        self.url_list = []
        for i in range(len(c)):
            self.url_list.append(c[i])

        k = 'loginPage'
        c = cfg.get(k)
        if (c==None or len(c)==0):
            logger.error("Configure ERROR: '{0}'".format(k))
            exit(-1)
        self.Login_Url = c

        k = 'billPage'
        c = cfg.get(k)
        if (c==None or len(c)==0):
            logger.error("Configure ERROR: '{0}'".format(k))
            exit(-1)
        self.Bill_Url = c

        k = 'accountPage'
        c = cfg.get(k)
        if (c==None or len(c)==0):
            logger.error("Configure ERROR: '{0}'".format(k))
            exit(-1)
        self.Account_Url = c

        rds = cfg['redis']
        if len(rds)>0:
            self.myrds = myredis(cfg['redis']['address'],
                                 cfg['redis']['port'],
                                 cfg['redis']['succOrderCache'],
                                 cfg['redis']['failOrderCache'])
        

    def init_driver(self):
        # 初始化浏览器对象
        option = webdriver.ChromeOptions()
        #option.add_argument('disable-infobars')
        option.add_experimental_option('excludeSwitches', ['enable-automation'])
        option.add_argument('--disable-extensions') 
        option.add_argument('--profile-directory=Default') 
        option.add_argument("--incognito") 
        option.add_argument("--disable-plugins-discovery")
        option.add_argument("--start-maximized")
        self.sel = webdriver.Chrome(options=option)
        self.sel.maximize_window()

 
    def wait_input(self, ele, str):
        for i in str:
            ele.send_keys(i)
            time.sleep(random.uniform(0.1,0.5))


    def pwd_login(self):
        """
          FIXME: 怎么绕过 支付宝 账密登陆的 反爬检测？
          verifyId.json  做了 什么 检测  操作？
        """
        sel = self.sel
        sel.implicitly_wait(3)
        time.sleep(1)
        # 找到用户名字输入框
        sel.find_element_by_xpath('//*[@data-status="show_login"]').click()
        uname = sel.find_element_by_id('J-input-user')
        uname.clear()
        uname.click()
        time.sleep(1000)
        logger.debug('input login account.....')
        self.wait_input(uname, self.user)
        sel.implicitly_wait(5)
        time.sleep(1)
        # 找到密码输入框
        upass = sel.find_element_by_id('password_rsainput')
        upass.clear()
        logger.debug('input password....')
        self.wait_input(upass, self.passwd)
        sel.implicitly_wait(10)
        time.sleep(3)
        # 找到登录按钮
        #time.sleep(1)
        butten = sel.find_element_by_id('J-login-btn')
        butten.click()
 
        # sel.save_screenshot('2.png')
        logger.debug(sel.current_url)

   
    def get_login_name(self):
        sel = self.sel

        sel.get(self.Account_Url)
        logger.debug("Getting account name...")

        try:
            s = 'span[class~=\'ft-14\']'
            wait = ui.WebDriverWait(sel,random.randint(self.minDelay,self.maxDelay))
            wait.until(lambda driver: sel.find_element_by_css_selector(s))
            self.loginName = sel.find_element_by_css_selector(s).text.split(' ')[0]

            logger.debug("login name is: " + self.loginName)

        except Exception as e:
            logger.debug("login name not found, {0}".format(e))



    def enable_chan(self,enable):
        if self.loginName==None or len(self.loginName)==0:
            logger.warn("no login name detected!!")
            return

        req_data = 'enable=' + str(enable) + '&mch_id=' + 'mch_001' + '&loginName=' + self.loginName 

        sen = 'enable'
        if enable==0:
            sen = 'disable'
           
        logger.debug("*** To {0} pay channel by login name '{1}'" \
                     .format(sen,self.loginName))

        req_data = bytes(req_data,encoding='utf-8')
        req = urllib.request.Request(self.enableChanUrl,data=req_data)
        res = urllib.request.urlopen(req).read()
        res = str(res).replace('b','').replace('\'','')
        logger.debug("response：{0}".format(res))



    def qrcode_login(self):
        sel = self.sel

        sel.get(self.Login_Url)
        logger.debug("Try to re-login...")

        while True:
            try:
                if sel.find_element_by_xpath('//*[@data-status="show_login"]'):
                    #logger.debug("current url: " + sel.current_url)
                    time.sleep(3)
            except NoSuchElementException:
                logger.debug("Login OK!")
                break


    def send_bill_notify(self,notify_data):

        if notify_data==None:
            return 

        logger.debug("Sending notification: {0}".format(notify_data))

        try:
            rds = self.myrds
            out_trade_no= notify_data['out_trade_no']
            trade_no    = notify_data['trade_no']

            stored = rds.read(rds.succCache,out_trade_no)
            if stored!=None:

                stored = str(stored).replace('b','').replace('\'','')

                if stored == trade_no:
                    logger.debug("out_trade_no '{0}' with trade_no '{1}' duplicates, will NOT notify again!"\
                                 .format(out_trade_no,trade_no))
                    return

                logger.debug("out_trade_no '{0}' already has an old trade_no '{1}', will be over-written by '{2}'"\
                             .format(out_trade_no,stored,trade_no))


            notify_data= parse.urlencode(notify_data).encode('utf-8')
            req = urllib.request.Request(self.notifyUrl,data=notify_data)
            res = urllib.request.urlopen(req).read()
            res = str(res).replace('b','').replace('\'','')
            logger.debug("response：{0}".format(res))

            if res == 'success':
              logger.debug("notify order '{0}' ok!!".format(out_trade_no))
              rds.write(rds.succCache,out_trade_no,trade_no)
            else:
              logger.error("notify order '{0}' fail: {1}".format(out_trade_no,res))
              rds.write(rds.failCache,out_trade_no,trade_no)

        except ConnectionRefusedError as e:
            logger.error("can't connect to " + self.notifyUrl)
        except urllib.error.URLError as e:
            logger.error("can't connect to " + self.notifyUrl)
        # XXX: what the fuck 'http.client.RemoteDisconnected' is???
        except Exception as e:
            logger.error(e)

    def parse_page(self):

        sel = self.sel

        num = 0
        while True:
            try:
                num = num + 1
                al_day_xpath = '//*[@id="J-item-' + str(num) + '"]/td[1]/p[1]'      #日期
                al_time_xpath = '//*[@id="J-item-' + str(num) + '"]/td[1]/p[2]'     #时刻
                al_way_xpath = '//*[@id="J-item-' + str(num) + '"]/td[3]/p[1]'      #方式
                al_payer_xpath = '//*[@id="J-item-' + str(num) + '"]/td[5]/p[1]'    #付款人
                al_snum_xpath = '//*[@id="J-item-' + str(num) + '"]/td[4]/p[1]'     #流水号
                al_figure_xpath = '//*[@id="J-item-' + str(num) + '"]/td[6]/span'   #金额
                al_status_xpath = '//*[@id="J-item-' + str(num) + '"]/td[8]/p[1]'   #交易状态

                wait = ui.WebDriverWait(sel,random.randint(self.minDelay,self.maxDelay))
                wait.until(lambda driver: sel.find_element_by_xpath(al_day_xpath))
                al_day = sel.find_element_by_xpath(al_day_xpath).text

                wait.until(lambda driver: sel.find_element_by_xpath(al_time_xpath))
                al_time = sel.find_element_by_xpath(al_time_xpath).text

                wait.until(lambda driver: sel.find_element_by_xpath(al_way_xpath))
                al_way = sel.find_element_by_xpath(al_way_xpath).text

                wait.until(lambda driver: sel.find_element_by_xpath(al_payer_xpath))
                al_payer = sel.find_element_by_xpath(al_payer_xpath).text

                wait.until(lambda driver: sel.find_element_by_xpath(al_snum_xpath))
                al_snum = sel.find_element_by_xpath(al_snum_xpath).text

                wait.until(lambda driver: sel.find_element_by_xpath(al_figure_xpath))
                al_figure = sel.find_element_by_xpath(al_figure_xpath).text

                wait.until(lambda driver: sel.find_element_by_xpath(al_status_xpath))
                al_status = sel.find_element_by_xpath(al_status_xpath).text

                notify_data = self.construct_notify(al_day,al_time,al_way,al_payer,al_snum,al_figure,al_status)
                
                # send notify
                self.send_bill_notify(notify_data)

            except NoSuchElementException as e :
                #logger.debug(e)
                logger.debug("Scanning current page DONE!")
                break 
            except TimeoutException as e1:
                #logger.debug(e1)
                logger.debug("Scanning current page DONE! Time out!!")
                break 


    def parse_bill(self):
        sel = self.sel

        sel.get(self.Bill_Url)
        sel.implicitly_wait(3)
        time.sleep(3)
        try:
            sel.find_element_by_xpath('//*[@id="J-item-1"]')
        except:
            logger.debug("Login fail!!!")
            return -1

        page = 1
        while True:
            try:

              """查询一页"""
              logger.info("Page{0} ...".format(page))
              self.parse_page()

              """下一页"""
              """ <a>标签 class page-next"""
              s = 'a.page-next'
              wait = ui.WebDriverWait(sel,random.randint(self.minDelay,self.maxDelay))
              wait.until(lambda driver: sel.find_element_by_css_selector(s))
              sel.find_element_by_css_selector(s).click()

              page = page + 1

            except NoSuchElementException as e :
                #logger.debug(e)
                logger.debug("End of Bill")
                break 
            except TimeoutException as e1:
                #logger.debug(e1)
                logger.debug("End of Bill, Time out!!!")
                break 

        """
        self.myrds.save()
        logger.debug("flushing redis...")
        """

        return 0

 
    def construct_notify(self,date,time,way,payer,snum,figure,status):
        sign = ''
        amount = 0
        st = 'TRADE_ERROR'
        otnkey = self.otnKeyword

        logger.debug("***Bill record{0},{1},{2},{3},{4}".format(date+time,way,snum,status,payer))

        # 以特定字符串开头的备注才认为是有效的
        #   个人 转帐  记录，才 进行 后续 处理
        if otnkey not in way:
            return None
            
        otn = way[len(otnkey):]

        tradeNo = ''
        snl = snum.split('|')
        if len(snl)>0:
            tradeNo = snl[0][len('订单号:'):]

        if '成功' in status:
            st = 'TRADE_SUCCESS'
        else:
            st = 'TRADE_ERROR'

        if '+' in figure:
            sign = 'in'
        elif '-' in figure:
            sign = 'out'

        amount = figure[2:]

        return {'gmt_payment' : date.replace('.','-')+"_"+time,
                'seller_id'   : payer,
                'amount'      : amount,
                'trade_status': st,
                'out_trade_no': otn,
                'trade_no'    : tradeNo
                }


 
    def get_data(self):
        sel = self.sel

        while True:
            try:
                # 跳转到账单页面
                logger.debug('Start scanning Bill....')
                if self.parse_bill()==-1:
                    # disble thsi channel
                    self.enable_chan(0)

                    # re-login
                    self.qrcode_login()
                    time.sleep(random.randint(self.minDelay,self.maxDelay))
     
                    continue


                self.get_login_name()
                #time.sleep(random.randint(self.minDelay,self.maxDelay))
                    
                # enable the latest login chan
                self.enable_chan(1)
                time.sleep(random.randint(self.minDelay,self.maxDelay))
                 
                pgtotal = random.randint(1,len(self.url_list))
                logger.debug('Anti-anti-papa total {0} pages....'.format(pgtotal))

                for i in range(pgtotal):
                    page = self.url_list[random.randint(0,len(self.url_list)-1)]
                    logger.debug('Jumpping to page...{0}'.format(page))
                    sel.get(page)
                    time.sleep(random.randint(self.minDelay,self.maxDelay))

                logger.debug('Anti-anti-papa Done!')

            except Exception as e:
                logger.error(e)
                traceback.print_exc()



def init_log(logpath):
    logger.setLevel(logging.DEBUG)

    formatter = logging.Formatter('%(asctime)s - %(module)s(%(process)d)%(funcName)s.%(lineno)d - %(levelname)s - %(message)s')

    if (len(logpath)>0):
      fh = logging.handlers.TimedRotatingFileHandler((logpath+"/"+globalName+".log"),when='D')
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
  cont= ""
  cfg = os.path.dirname(os.path.realpath(__file__)) + "/abc_conf.json"

  opts, args = getopt.getopt(sys.argv[1:], "L:c:")
  logpath = ''
  for a,opt in opts:
    if a=='-L':
      logpath = opt
    elif a=='-c':
      cfg = opt

  logger = init_log(logpath)

  # read configs
  with open(cfg,"r") as m_fd:
    cont = m_fd.read()

  # initialize the interfaces
  bc = billCollector(cont) 
  bc.get_data()

  """
  data = {
     'gmt_payment':'2020-03-24 11:48',
     'seller_id':'惠州市好福来商贸有限公司',
     'amount':'0.01',
     'trade_status':'TRADE_SUCCESS',
     'out_trade_no':'44100-27209-001',
     'trade_no':'20200324200040011100140086003041'}
  bc.send_bill_notify(data)
  """

if __name__=="__main__":
  main()


