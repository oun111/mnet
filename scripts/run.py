#!/usr/bin/python2 -B

import os
from sys import argv
from multiprocessing import cpu_count

if len(argv)!=2:
  print("needs 1 argument")
  exit(-1)
  
script,opt = argv


logFlushInterval = 5
connTimeout = 1
numWorkers = 0  #cpu_count()
#logToFile = "-L /tmp/"
logToFile = ""


if opt=="0" or opt=="1":
  #os.system("rm -f /tmp/mnet_*.log")
  os.system("killall mnet_test")
  os.system("killall syncd.py")

  if opt=="1":
     os.system("sudo echo \"20000\" > /proc/sys/net/core/somaxconn")
     os.system("sudo echo \"20000\" > /proc/sys/net/ipv4/tcp_max_syn_backlog")
     os.system("ulimit -n 20000")

     os.system("./src/mnet_test -l ./modules/ -cp ./conf/paysvr_conf.json  -cm ./conf/mp2_conf.json -cj ./conf/myjvm_conf.json -mF {0} -mW {1} {2} -cTo {3}  &".format(logFlushInterval,numWorkers,logToFile,connTimeout))

     n = 0
     while (n<numWorkers):
       os.system("./src/level5/pay_svr/syncd.py -c ./conf/paysvr_conf.json {0}  &".format(logToFile))
       n = n+1

