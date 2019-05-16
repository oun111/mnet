#!/usr/bin/python2 -B

import os
from sys import argv
from multiprocessing import cpu_count

if len(argv)!=2:
  print("needs 1 argument")
  exit(-1)
  
script,opt = argv

##
# pressure: 
#
#    0 debug usage, single process, console output
#
#    1 pressure test, multiple processes, using log files
##
pressure = 1


logFlushInterval = 5
connTimeout = 3600*24

if pressure==1:
  numWorkers = cpu_count()
  logToFile = "-L /home/user1/work/mnet/logs/"
  numSyncds = numWorkers 
else:
  numWorkers = 0
  logToFile = ""
  numSyncds = 1


if opt=="0" or opt=="1":
  os.system("killall payd")
  os.system("killall syncd.py")

  if opt=="1":
     os.system("sudo echo \"20000\" > /proc/sys/net/core/somaxconn")
     os.system("sudo echo \"20000\" > /proc/sys/net/ipv4/tcp_max_syn_backlog")
     os.system("ulimit -n 20000")

     os.system("./src/mnet_test -l ./modules/ -cp ./conf/paysvr_conf.json  -cm ./conf/mp2_conf.json -cj ./conf/myjvm_conf.json -mF {0} -mW {1} {2} -cTo {3}  &".format(logFlushInterval,numWorkers,logToFile,connTimeout))

     n = 0
     while (n<numSyncds):
       os.system("./src/level5/pay_svr/syncd.py -c ./conf/paysvr_conf.json {0}  &".format(logToFile))
       n = n+1

