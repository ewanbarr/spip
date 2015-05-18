#!/usr/bin/env python

##############################################################################
#  
#     Copyright (C) 2015 by Andrew Jameson
#     Licensed under the Academic Free License version 2.1
# 
###############################################################################

#
# spip_lmc_monitor - 
#


import os, threading, sys, time, socket, select, signal, traceback
import spip

#################################################################
# host based monitoring thread
#   disk capacity
#   smrb capacities
#   load values
#   temperature values
#   fan speeds
#   gpu properties

# read disk capacity information for specified directories
def getDiskCapacity (dirs, dl):

  result = 0  
  disks = {}

  for dir in dirs:

    cmd = "df " + dir + " -B 1048576 -P | tail -n 1 | awk '{print $2,$3,$4}'"
    rval, lines = spip.system (cmd, 2 <= dl)
    result += rval

    if rval == 0 and len(lines) == 1:
      parts = lines[0].split()
      disks[dir] = {"size": parts[0], "used": parts[1], "available": parts[2]}

  return result, disks

# request SMRB state from SMRB monitoring points
def getSMRBCapacity (stream_ids, dl):

  smrbs = {}
  rval = 0

  for stream_id in stream_ids:
    port = spip_smrb.getDBMonPort (stream_id)
    sock = spip.openSocket (dl, "localhost", port)
    sock.send("smrb_status")
    data = sock.recv(65536)
    smrbs[stream_id] = json.loads(data) 
    sock.close()

  return rval, smrbs 

class monThread (threading.Thread):

  def __init__(self, quit_event, streams, dl):
    threading.Thread.__init__(self)
    self.quit_event = quit_event
    self.streams = streams

  def run (self):

    print "run"

if __name__ == "__main__":
  test_dl = 2

  spip.logMsg(2, test_dl, "getting disk capcity for /")
  rval, disks = getDiskCapacity ("/", 3)
  spip.logMsg(2, test_dl, "rval="+str(rval))
  spip.logMsg(2, test_dl, "disks="+str(disks))

  spip.logMsg(2, test_dl, "reading SMRB info")
  rval, smrbs = getSMRBCapacity (stream_ids, dl)

  sys.exit(0)
    

