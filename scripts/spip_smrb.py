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
import json
import spip

SCRIPT = "spip_smrb"
DL     = 2

def getDBKey (inst_id, stream_id, num_stream, db_id):
  index = (int(db_id) * int(num_stream)) + int(stream_id)
  db_key = inst_id + "%03x" % (2 * index)
  return db_key

def getDBMonPort (stream_id):
  start_port = 20000  
  return start_port + int(stream_id)

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

  def __init__ (self, keys, stream_id, quit_event):
    threading.Thread.__init__(self)
    self.keys = keys
    self.stream_id = stream_id
    self.quit_event = quit_event
    self.poll = 5

  def run (self):

    can_read = []
    can_write = []
    can_error = []

    try:
      spip.logMsg(2, DL, "monThread: launching")

      spip.logMsg(1, DL, "monThread: opening mon socket")
      sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
      sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
      port = getDBMonPort(self.stream_id)
      spip.logMsg(2, DL, "monThread: binding to localhost:" + str(port))
      sock.bind(("localhost", port))

      sock.listen(2)

      can_read.append(sock)
      timeout = 1

      while (not self.quit_event.isSet()): 

        smrb = {}

        for key in self.keys:

          # read the current state of the data block
          rval, hdr, data = getDBState(key)
          smrb[key] = {'hdr': hdr, 'data' : data}
     
        serialized = json.dumps(smrb)
  
        spip.logMsg(3, DL, "monThread: calling select len(can_read)=" + 
                    str(len(can_read)))
        timeout = self.poll
        did_read, did_write, did_error = select.select(can_read, can_write, 
                                                       can_error, timeout)
        spip.logMsg(3, DL, "monThread: read="+str(len(did_read)) + 
                    " write="+str(len(did_write))+" error="+str(len(did_error)))

        if (len(did_read) > 0):
          for handle in did_read:
            if (handle == sock):
              (new_conn, addr) = sock.accept()
              spip.logMsg(1, DL, "monThread: accept connection from " + 
                          repr(addr))
              can_read.append(new_conn)

            else:
              message = handle.recv(4096)
              message = message.strip()
              spip.logMsg(3, DL, "monThread: message='" + message+"'")
              if (len(message) == 0):
                spip.logMsg(1, DL, "monThread: closing connection")
                handle.close()
                for i, x in enumerate(can_read):
                  if (x == handle):
                    del can_read[i]
              else:
                if message == "smrb_status":
                  print smrb
                  handle.send(serialized)
          
      for i, x in enumerate(can_read):
        x.close()
        del can_read[i]

    except:
      self.quit_event.set()
      spip.logMsg(1, DL, "monThread: exception caught: " +
                  str(sys.exc_info()[0]))
      print '-'*60
      traceback.print_exc(file=sys.stdout)
      print '-'*60
      for i, x in enumerate(can_read):
        x.close()
        del can_read[i]

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

    def signal_handler(signal, frame):
      spip.logMsg(0, DL, "spip_smrb: CTRL+C")
      quit_event.set()

    signal.signal(signal.SIGINT, signal_handler)

    # start a control thread to handle quit requests
    control_thread = spip.controlThread(quit_file, pid_file, quit_event, DL)
    control_thread.start()

    # get a list of data block ids
    db_ids = cfg["DATA_BLOCK_IDS"].split(" ")
    db_prefix = cfg["DATA_BLOCK_PREFIX"]
    num_stream = cfg["NUM_STREAM"]
    db_keys = []

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

      db_keys.append(db_key)

    # after creation, launch thread to monitor smrb, maintaining state
    mon_thread = monThread(db_keys, stream_id, quit_event)
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

