#!/usr/bin/python2 -B

import os
from sys import argv
from multiprocessing import cpu_count

if len(argv)!=2:
  print("needs 1 argument")
  exit(-1)
  
script,opt = argv


basepath = "/home/kf/dist/"

app = ("mnet_test")
syncd = ("syncd.py")
conf= (basepath + "paysvr_conf.json")
mod = basepath
logFlushInterval = 5
numWorkers = 0
logToFile = "-L {0}/logs".format(basepath)
#logToFile = ""
numSyncds = cpu_count()


if opt=="0" or opt=="1":
  os.system(("killall " + "payd"))
  os.system(("killall " + syncd))

  if opt=="1":
     os.system("sudo echo \"20000\" > /proc/sys/net/core/somaxconn")
     os.system("sudo echo \"20000\" > /proc/sys/net/ipv4/tcp_max_syn_backlog")
     os.system("ulimit -n 20000")

     run = (basepath + app)
     os.system("{0} -l {1} -cp {2} -mF {3} -mW {4} {5} &".format(run,mod,conf,logFlushInterval,numWorkers,logToFile))

     n = 0
     run = (basepath + syncd)
     while (n<numSyncds):
       os.system("{0} -c {1} {2} &".format(run,conf,logToFile))
       n = n+1

