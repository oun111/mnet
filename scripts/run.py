#!/usr/bin/python2 -B

import os
from sys import argv

if len(argv)!=2:
  print("needs 1 argument")
  exit(-1)
  
script,opt = argv


logFlushInterval = 5
numWorkers = 0
#logToFile = "-L /tmp/"
logToFile = ""


if opt=="0" or opt=="1":
  os.system("rm -f /tmp/mnet_*.log")
  os.system("killall mnet_test")

  if opt=="1":
     os.system("sudo echo \"20000\" > /proc/sys/net/core/somaxconn")
     os.system("sudo echo \"20000\" > /proc/sys/net/ipv4/tcp_max_syn_backlog")
     os.system("ulimit -n 20000")

     os.system("./src/mnet_test -l ./modules/ -cp ./conf/paysvr_conf.json  -cm ./conf/mp2_conf.json -cj ./conf/myjvm_conf.json -mF {0} -mW {1} {2} ".format(logFlushInterval,numWorkers,logToFile))

