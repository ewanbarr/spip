###############################################################################
#  
#     Copyright (C) 2015 by Andrew Jameson
#     Licensed under the Academic Free License version 2.1
# 
###############################################################################

from datetime import datetime
from datetime import timedelta

def getCurrentTimeUS():
  now = datetime.today()
  now_str = now.strftime("%Y-%m-%d-%H:%M:%S.%f")
  return now_str

def getCurrentTime(toadd=0):
  now = datetime.today()
  if (toadd > 0):
    delta = timedelta(0, toadd)
    now = now + delta
  now_str = now.strftime("%Y-%m-%d-%H:%M:%S")
  return now_str

def getUTCTime(toadd=0):
  now = datetime.utcnow()
  if (toadd > 0):
    delta = timedelta(0, toadd)
    now = now + delta
  now_str = now.strftime("%Y-%m-%d-%H:%M:%S")
  return now_str

def convertLocalToUnixTime(epoch_str):
  epoch = datetime.strptime(epoch_str, "%Y-%m-%d-%H:%M:%S")
  return epoch.strftime('%s')

def convertUTCToUnixTime(epoch_str):
  epoch = datetime.strptime(epoch_str + " UTC", "%Y-%m-%d-%H:%M:%S %Z")
  return epoch.strftime('%s')

def diffUTCTimes(epoch1_str, epoch2_str):
  epoch1 = datetime.strptime(epoch1_str+ " UTC", "%Y-%m-%d-%H:%M:%S %Z")
  epoch2 = datetime.strptime(epoch2_str+ " UTC", "%Y-%m-%d-%H:%M:%S %Z")
  delta = epoch2 - epoch1
  return delta.seconds

def diffUTCTime(epoch_str):
  epoch = datetime.strptime(epoch_str+ " UTC", "%Y-%m-%d-%H:%M:%S %Z")
  now = datetime.utcnow()
  delta = now - epoch
  return delta.seconds
