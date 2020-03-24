#!/usr/bin/python3 -B

import os
import sys
import json
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


globalName = 'billCollector'
logger     = logging.getLogger(globalName + '_log')

 
class billCollector(object):
 
    def __init__(self,cfgContent,user='', passwd=''):
        # 登录 url
        self.Login_Url = 'https://auth.alipay.com/login/index.htm'
        # 账单 url
        self.Bill_Url = 'https://consumeprod.alipay.com/record/advanced.htm'
        # 防封页面列表，随机访问
        self.url_list = [
          "https://b.alipay.com/signing/productSetV2.htm?mrchportalwebServer=https%3A%2F%2Fmrchportalweb.alipay.com",
          "https://mrchportalweb.alipay.com/user/asset/flow.htm#/flow/list",
          "https://bizfundprod.alipay.com/fund/autoFinance/index.htm#/?_k=oz4sr1",
          "https://my.alipay.com/portal/i.htm",
          "https://app.alipay.com/container/web/index.htm",
          "https://my.alipay.com/portal/account/safeguard.htm",
          "https://zht.alipay.com/asset/newIndex.htm",
          "https://my.alipay.com/wealth/index.html",
          "https://help.alipay.com/lab/question.htm",
          "https://yebprod.alipay.com/yeb/asset.htm",
          "https://zcbprod.alipay.com/index.htm",
          "https://zd.alipay.com/ebill/monthebill.htm",
        ]

        self.parse_configs(cfgContent)

        self.user = user
        self.passwd = passwd
        self.init_driver()
         

    def parse_configs(self,cont):
        cfg = json.loads(cont)

        k = 'notifyUrl'
        c = cfg.get(k)
        if (c==None or len(c)==0):
            logger.error("配置错误: '{0}'".format(k))
            exit(-1)
        self.notifyUrl = c

        k = 'safeDelayRange'
        c = cfg.get(k)
        if (c==None or len(c)==0):
            logger.error("配置错误: '{0}'".format(k))
            exit(-1)
        rl= c.split(',')
        if len(rl)!=2:
            logger.error("配置错误1: '{0}'".format(k))
            exit(-1)

        self.minDelay = int(rl[0])
        self.maxDelay = int(rl[1])

        k = 'otnKeyword'
        c = cfg.get(k)
        if (c==None or len(c)==0):
            logger.error("配置错误: '{0}'".format(k))
            exit(-1)
        self.otnKeyword = c

        logger.info("通知地址：" + self.notifyUrl)
        logger.info("安全延迟时间区间：({0},{1})".format(self.minDelay,self.maxDelay))
        logger.info("订单区分标志：" + self.otnKeyword)
        

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
        logger.debug('正在输入账号.....')
        self.wait_input(uname, self.user)
        sel.implicitly_wait(5)
        time.sleep(1)
        # 找到密码输入框
        upass = sel.find_element_by_id('password_rsainput')
        upass.clear()
        logger.debug('正在输入密码....')
        self.wait_input(upass, self.passwd)
        sel.implicitly_wait(10)
        time.sleep(3)
        # 找到登录按钮
        #time.sleep(1)
        butten = sel.find_element_by_id('J-login-btn')
        butten.click()
 
        # sel.save_screenshot('2.png')
        logger.debug(sel.current_url)


    def qrcode_login(self):
        sel = self.sel

        sel.get(self.Login_Url)
        logger.debug("尝试重新登陆...")

        while True:
            try:
                if sel.find_element_by_xpath('//*[@data-status="show_login"]'):
                    #logger.debug("current url: " + sel.current_url)
                    time.sleep(3)
            except NoSuchElementException:
                logger.debug("成功登陆")
                break


    def send_bill_notify(self,notify_data,out_trade_no):

        if notify_data==None:
            return 

        logger.debug("尝试发送通知: ",notify_data)

        try:
            notify_data= parse.urlencode(notify_data).encode('utf-8')
            req = urllib.request.Request(self.notifyUrl,data=notify_data)
            res = urllib.request.urlopen(req).read()
            res = str(res).replace('b','').replace('\'','')
            logger.debug("response：{0}".format(res))

            if res == 'success':
              logger.debug("notify order '{0}' ok!!".format(out_trade_no))
            else:
              logger.error("notify order '{0}' fail: {1}".format(out_trade_no,res))

        except ConnectionRefusedError as e:
            logger.error("can't connect to " + self.notifyUrl)
        except urllib.error.URLError as e:
            logger.error("can't connect to " + self.notifyUrl)
            

        


    def parse_bill(self):
        sel = self.sel

        sel.get(self.Bill_Url)
        sel.implicitly_wait(3)
        time.sleep(3)
        try:
            sel.find_element_by_xpath('//*[@id="J-item-1"]')
        except:
            logger.debug("登录出错啦！")
            return -1

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
                logger.debug("扫描结束")
                break 
            except TimeoutException as e1:
                #logger.debug(e1)
                logger.debug("扫描结束, 超时")
                break 

        return 0

 
    def construct_notify(self,date,time,way,payer,snum,figure,status):
        sign = ''
        amount = 0
        st = 'TRADE_ERROR'
        otnkey = self.otnKeyword

        # 以特定字符串开头的备注才认为是有效的
        #   个人 转帐  记录，才 进行 后续 处理
        if otnkey not in way:
            return None,None
            
        otn = way[len(otnkey):]

        tradeNo = ''
        snl = snum.split('|')
        if len(snl)>0:
            tradeNo = snl[0][len('订单号:'):]

        if '成功' in status:
            st = 'TRADE_SUCCESS'

        if '+' in figure:
            sign = 'in'
        elif '-' in figure:
            sign = 'out'

        amount = figure[2:]

        return {'gmt_payment=' : date.replace('.','-')+" "+time,
                'seller_id='   : payer,
                'amount='      : amount,
                'trade_status=': st,
                'out_trade_no=': otn,
                'trade_no='    : tradeNo
                }


 
    def get_data(self):
        sel = self.sel

        while True:
            # 跳转到账单页面
            logger.debug('正在跳转页面....' + self.Bill_Url)
            if self.parse_bill()==-1:
                self.qrcode_login()
                time.sleep(self.maxDelay)
                continue

            time.sleep(random.randint(self.minDelay,self.maxDelay))

            page = self.url_list[random.randint(0,len(self.url_list)-1)]
            logger.debug('正在跳转页面（反反爬）....' + page)
            sel.get(page)
            time.sleep(random.randint(self.minDelay,self.maxDelay))



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
     'out_trade_no':'44100-27204',
     'trade_no':'20200324200040011100140086003041'}
  bc.send_bill_notify(data,data['out_trade_no'])
  """

if __name__=="__main__":
  main()


