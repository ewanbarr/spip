#!/usr/bin/env python

##############################################################################
#  
#     Copyright (C) 2015 by Andrew Jameson
#     Licensed under the Academic Free License version 2.1
# 
###############################################################################
#
#   configured SMRBs initialized on launch
#   monitoring of SMRBs in separate threads
#   SMRBs destroyed upon exit
#

import os, threading, sys, time, socket, select, signal, traceback
import spip

SCRIPT = "spip_smrb"
DL     = 2

def signal_handler(signal, frame):
  spip.logMsg(0, DL, "spip_smrb: CTRL+C")
  global quit_event
  quit_event.set()

def getDBKey (inst_id, stream_id, num_stream, db_id):
  index = (int(db_id) * int(num_stream)) + int(stream_id)
  db_key = inst_id + "%03x" % (2 * index)
  return db_key

def getDBState (key):
  cmd = "dada_dbmetric -k " + key
  rval, lines = spip.system (cmd, 2 <= DL) 
  if rval == 0:
    a = lines[0].split(',')
    hdr = {'nbufs':a[0], 'full':a[1], 'clear':a[2], 'written':a[3],'read':a[4]}
    dat = {'nbufs':a[5], 'full':a[6], 'clear':a[7], 'written':a[8],'read':a[9]}
    return 0, hdr, dat
  else:
    return 1, {}, {}

class monThread (threading.Thread):

  def __init__ (self, key, quit_event):
    threading.Thread.__init__(self)
    self.key = key
    self.quit_event = quit_event
    self.poll = 5

  def run (self):
    key = self.key
    
    try:
      spip.logMsg(1, DL, "["+key+"] monThread launching")

      while (not quit_event.isSet()): 
        
        # read the current state of the data block
        rval, hdr, data = getDBState(self.key)
       
        # TODO mechanism to expose this information to something that
        # wants it? a separate comms thread perhaps 

        remaining = self.poll
        while (not quit_event.isSet() and remaining > 0):
          time.sleep (1)
          remaining -= 1

    except:
      spip.logMsg(1, DL, "monThread ["+key+"] exception caught: " +
                  str(sys.exc_info()[0]))
      print '-'*60
      traceback.print_exc(file=sys.stdout)
      print '-'*60

######################################################################
# main

def main (argv):

  # this should come from command line argument
  stream_id = argv[1]

  # read configuration file
  cfg = spip.getConfig()

  control_thread = []
  mon_threads = []

  log_file  = cfg["SERVER_LOG_DIR"] + "/" + SCRIPT + ".log"
  pid_file  = cfg["SERVER_CONTROL_DIR"] + "/" + SCRIPT + ".pid"
  quit_file = cfg["SERVER_CONTROL_DIR"] + "/"  + SCRIPT + ".quit"

  if os.path.exists(quit_file):
    sys.stderr.write("quit file existed at launch: " + quit_file)
    sys.exit(1)

  # become a daemon
  # spip.daemonize(pid_file, log_file)

  try:

    spip.logMsg(1, DL, "STARTING SCRIPT")

    quit_event = threading.Event()

    signal.signal(signal.SIGINT, signal_handler)

    # start a control thread to handle quit requests
    control_thread = spip.controlThread(quit_file, pid_file, quit_event, DL)
    control_thread.start()

    # get a list of data block ids
    db_ids = cfg["DATA_BLOCK_IDS"].split(" ")
    db_prefix = cfg["DATA_BLOCK_PREFIX"]
    num_stream = cfg["NUM_STREAM"]

    for db_id in db_ids:
      db_key = getDBKey (db_prefix, stream_id, num_stream, db_id)
      spip.logMsg(0, DL, "spip_smrb: db_key for " + db_id + " is " + db_key)

      nbufs = cfg["BLOCK_NBUFS_" + db_id]
      bufsz = cfg["BLOCK_BUFSZ_" + db_id]
      nread = cfg["BLOCK_NREAD_" + db_id]
      page  = cfg["BLOCK_PAGE_" + db_id]

      cmd = "dada_db -k " + db_key + " -n " + nbufs + " -b " + bufsz + " -r " + nread
      if page:
        cmd += " -p -l"
      rval, lines = spip.system (cmd, 2 <= DL)

      # after creation, launch thread to monitor smrb, maintaining state
      mon_thread = monThread(db_key, quit_file)
      mon_thread.start()
      mon_threads.append(mon_thread)

    while (not quit_event.isSet()):
      time.sleep(1)

    for db_id in db_ids:
      db_key = getDBKey (db_prefix, stream_id, num_stream, db_id)
      cmd = "dada_db -k " + db_key + " -d"
      rval, lines = spip.system (cmd, 2 <= DL)

  except:
    spip.logMsg(-2, DL, "main: exception caught: " + str(sys.exc_info()[0]))
    print '-'*60
    traceback.print_exc(file=sys.stdout)
    print '-'*60
    quit_event.set()

  # join threads
  spip.logMsg(2, DL, "main: joining control thread")
  if (control_thread):
    control_thread.join()

  spip.logMsg(2, DL, "main: joining " + str(len(db_ids)) + " mon threads")
  for i in range(len(db_ids)):
    mon_threads[i].join()

  spip.logMsg(1, DL, "STOPPING SCRIPT")

if __name__ == "__main__":
  if len(sys.argv) != 2:
    print "ERROR: 1 command line argument expected"
    sys.exit(1)

  main (sys.argv)
  sys.exit(0)

