#!/usr/bin/python3 -B
from selenium import webdriver
#from DbHandle import DbHandle
import time
 
# 登录 url
Login_Url = 'https://auth.alipay.com/login/index.htm?goto=https%3A%2F%2Fwww.alipay.com%2F'
# 账单 url
Bill_Url = 'https://consumeprod.alipay.com/record/standard.htm'
 
class Alipay_Bill_Info(object):
 
    def __init__(self,user, passwd):
        self.user = user
        self.passwd = passwd
 
    def wait_input(self, ele, str):
        for i in str:
            ele.send_keys(i)
            time.sleep(0.5)
 
    def get_data(self):
        # 初始化浏览器对象
        sel = webdriver.Chrome()
        sel.maximize_window()
        sel.get(Login_Url)
        sel.implicitly_wait(3)
        time.sleep(1)
        # 找到用户名字输入框
        sel.find_element_by_xpath('//*[@data-status="show_login"]').click()
        uname = sel.find_element_by_id('J-input-user')
        uname.clear()
        print('正在输入账号.....')
        self.wait_input(uname, self.user)
        time.sleep(1)
        # 找到密码输入框
        upass = sel.find_element_by_id('password_rsainput')
        upass.clear()
        print('正在输入密码....')
        self.wait_input(upass, self.passwd)
        # 找到登录按钮
        time.sleep(1)
        butten = sel.find_element_by_id('J-login-btn')
        butten.click()
 
        # sel.save_screenshot('2.png')
        print(sel.current_url)
        # 跳转到账单页面
        print('正在跳转页面....')
        sel.get(Bill_Url)
        sel.implicitly_wait(3)
        time.sleep(2)
        try:
            sel.find_element_by_xpath('//*[@id="J-item-1"]')
        except:
            print("登录出错！")
            sel.close()
            return
        """
        db = DbHandle()
        sql = "SELECT set_date FROM ali_pay_bill ORDER BY bid DESC LIMIT 1"
        print(sql)
        dateline = db.select_one(sql)
        result = dateline['set_date']
        result = result.replace('.', '')
        result = result.replace('-', '')
        result = result.replace(':', '')
        dateline = int(result[2:])
        print(dateline)
        """
        for num in range(10):
            num = num + 1
            al_day_xpath = '//*[@id="J-item-' + str(num) + '"]/td[1]/p[1]'      #日期
            al_time_xpath = '//*[@id="J-item-' + str(num) + '"]/td[1]/p[2]'     #时刻
            al_way_xpath = '//*[@id="J-item-' + str(num) + '"]/td[3]/p[1]'      #方式
            al_payee_xpath = '//*[@id="J-item-' + str(num) + '"]/td[5]/p[1]'    #收款人
            al_snum_xpath = '//*[@id="J-item-' + str(num) + '"]/td[4]/p[1]'     #流水号
            al_figure_xpath = '//*[@id="J-item-' + str(num) + '"]/td[6]/span'   #金额
            al_status_xpath = '//*[@id="J-item-' + str(num) + '"]/td[8]/p[1]'   #交易状态
            al_day = sel.find_element_by_xpath(al_day_xpath).text
            al_time = sel.find_element_by_xpath(al_time_xpath).text
            al_way = sel.find_element_by_xpath(al_way_xpath).text
            al_payee = sel.find_element_by_xpath(al_payee_xpath).text
            al_snum = sel.find_element_by_xpath(al_snum_xpath).text
            al_figure = sel.find_element_by_xpath(al_figure_xpath).text
            al_status = sel.find_element_by_xpath(al_status_xpath).text
            single_data = self.handle_text(al_day,al_time,al_way,al_payee,al_snum,al_figure,al_status)
            print(single_data)
            """
            if(single_data['set_time']<=dateline):
                break
            sql = "INSERT INTO ali_pay_bill (set_date, \
                   other, amount, sign,statu,way,code) \
                   VALUES ('%s', '%s', %s, %s, %s, '%s', '%s' )" % \
                  (single_data['set_date'], single_data['other'], single_data['amount'], \
                   single_data['sign'], single_data['statu'], single_data['way'], single_data['code'])
            print(sql)
            db.update(sql)
            """
        sel.close()
 
    def handle_text(self,set_date,set_time,way,payee,snum,figure,status):
        statu = 0
        sign = 2
        amount = 0
        if '成功' in status:
            statu = 1
        if '+' in figure:
            sign = 1
            amount = float(figure[2:])
        elif '-' in figure:
            sign = 0
            amount = float(figure[2:])
        al_day = set_date+"-"+set_time
        al_time = set_time.replace(':', '')
        al_time = int((set_date+al_time).replace('.','')[2:])
        return {'set_date':al_day,'set_time':al_time,'other':payee,'amount':amount,'sign':sign,'statu':statu,'way':way,'code':snum}



def main():

  # initialize the interfaces
  abi = Alipay_Bill_Info("zf_13192804289_10@163.com","zr368368") 
  abi.get_data()

  """
  zf_13192804289_01@163.com zr368368
  hzxyl2020_01@163.com      bb368368
  hzzy2019_01@163.com       cc368368
  Hzkl2019_01@163.com       cc368368
  Hzhfl2020_01@163.com      bb368368
  """

if __name__=="__main__":
  main()


