#!/usr/bin/python3 -B

from selenium import webdriver
from selenium.common.exceptions import NoSuchElementException
from selenium.common.exceptions import TimeoutException
import selenium.webdriver.support.ui as ui
import time
import random

 
# 登录 url
#Login_Url = 'https://auth.alipay.com/login/index.htm?goto=https%3A%2F%2Fwww.alipay.com%2F'
Login_Url = 'https://auth.alipay.com/login/index.htm'
# 账单 url
#Bill_Url = 'https://consumeprod.alipay.com/record/standard.htm'
Bill_Url = 'https://consumeprod.alipay.com/record/advanced.htm'

# 防封页面列表，随机访问
url_list = [
  "https://b.alipay.com/signing/productSetV2.htm?mrchportalwebServer=https%3A%2F%2Fmrchportalweb.alipay.com",
  "https://mrchportalweb.alipay.com/user/asset/flow.htm#/flow/list",
  "https://bizfundprod.alipay.com/fund/autoFinance/index.htm#/?_k=oz4sr1",
]
 
class Alipay_Bill_Info(object):
 
    def __init__(self,user, passwd):
        self.user = user
        self.passwd = passwd
        self.init_driver()

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
        print('正在输入账号.....')
        self.wait_input(uname, self.user)
        sel.implicitly_wait(5)
        time.sleep(1)
        # 找到密码输入框
        upass = sel.find_element_by_id('password_rsainput')
        upass.clear()
        print('正在输入密码....')
        self.wait_input(upass, self.passwd)
        sel.implicitly_wait(10)
        time.sleep(3)
        # 找到登录按钮
        #time.sleep(1)
        butten = sel.find_element_by_id('J-login-btn')
        butten.click()
 
        # sel.save_screenshot('2.png')
        print(sel.current_url)


    def parse_bill(self):
        sel = self.sel

        sel.get(Bill_Url)
        sel.implicitly_wait(3)
        time.sleep(3)
        try:
            sel.find_element_by_xpath('//*[@id="J-item-1"]')
        except:
            print("登录出错！")
            #sel.close()
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

                wait = ui.WebDriverWait(sel,random.randint(10,30))
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

                single_data = self.handle_text(al_day,al_time,al_way,al_payer,al_snum,al_figure,al_status)
                print(single_data)

            except NoSuchElementException as e :
                #print(e)
                print("扫描结束")
                break 
            except TimeoutException as e1:
                #print(e1)
                print("扫描结束, 超时")
                break 

        return 0


 
    def get_data(self):

        sel = self.sel

        sel.get(Login_Url)

        while True:
            try:
                if sel.find_element_by_xpath('//*[@data-status="show_login"]'):
                    #print("current url: " + sel.current_url)
                    time.sleep(3)
            except NoSuchElementException:
                print("成功登陆")
                break

        while True:
            # 跳转到账单页面
            print('正在跳转页面....' + Bill_Url)
            if self.parse_bill()==-1:
                time.sleep(15)
                continue
            time.sleep(random.randint(10,30))

            page = url_list[random.randint(0,len(url_list)-1)]
            print('正在跳转页面....' + page)
            sel.get(page)
            time.sleep(random.randint(10,30))

        #sel.close()
 
    def handle_text(self,set_date,set_time,way,payer,snum,figure,status):
        statu = 0
        sign = 2
        amount = 0
        if '成功' in status:
            statu = 2
        if '+' in figure:
            sign = 1
            amount = float(figure[2:])
        elif '-' in figure:
            sign = 0
            amount = float(figure[2:])
        al_day = set_date+"-"+set_time
        al_time = set_time.replace(':', '')
        al_time = int((set_date+al_time).replace('.','')[2:])
        return {'set_date':al_day,'set_time':al_time,'payer':payer,'amount':amount,'sign':sign,'statu':statu,'remark':way,'tradeNo':snum}



def main():

  userDict = {
    "hzyc2019_01@163.com":"123456aA",
    "hzcx2019_01@163.com":"123456aA",
    "hzls2019_01@163.com":"dt368368",
    "hzhy2019_01@163.com":"dl417168",
    "15975532523":"123",
  }

  # initialize the interfaces
  k = "15975532523"
  abi = Alipay_Bill_Info(k,userDict[k]) 
  abi.get_data()


if __name__=="__main__":
  main()


