#!/usr/bin/python2 -B

import os
from sys import argv
from multiprocessing import cpu_count

if len(argv)!=2:
  print("needs 1 argument")
  exit(-1)
  
script,opt = argv


logFlushInterval = 5
numWorkers = 0
logToFile = "-L /home/user1/work/mnet/logs/"
#logToFile = ""


if opt=="0" or opt=="1":
  os.system("killall hsyncd")

  if opt=="1":
     os.system("./src/mnet_test -l ./modules/ -ch ./conf/hsd_conf.json -mF {0} -mW {1} {2}  &".format(logFlushInterval,numWorkers,logToFile))


