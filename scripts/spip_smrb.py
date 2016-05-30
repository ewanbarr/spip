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

import threading, sys, traceback, socket, select
from time import sleep

from json import dumps

from spip.daemons.bases import StreamBased
from spip.daemons.daemon import Daemon
from spip.log_socket import LogSocket
from spip.utils.core import system

DAEMONIZE = True
DL        = 1

class monThread (threading.Thread):

  def __init__ (self, keys, script):
    threading.Thread.__init__(self)
    self.keys = keys
    self.id = script.id
    self.quit_event = script.quit_event
    self.script = script
    self.poll = 5

  def run (self):

    can_read = []
    can_write = []
    can_error = []
    script = self.script

    try:
      script.log (2, "monThread: launching")

      script.log(2, "monThread: opening mon socket")
      sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
      sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
      port = SMRBDaemon.getDBMonPort(self.id)
      script.log(2, "monThread: binding to localhost:" + str(port))
      sock.bind(("localhost", port))

      sock.listen(2)

      can_read.append(sock)
      timeout = 1

      while (not self.quit_event.isSet()): 

        smrb = {}

        for key in self.keys:

          # read the current state of the data block
          rval, hdr, data = self.getDBState(key)
          smrb[key] = {'hdr': hdr, 'data' : data}
     
        # from json
        serialized = dumps(smrb)
  
        script.log(3, "monThread: calling select len(can_read)=" + 
                    str(len(can_read)))
        timeout = self.poll
        did_read, did_write, did_error = select.select(can_read, can_write, 
                                                       can_error, timeout)
        script.log(3, "monThread: read="+str(len(did_read)) + 
                    " write="+str(len(did_write))+" error="+str(len(did_error)))

        if (len(did_read) > 0):
          for handle in did_read:
            if (handle == sock):
              (new_conn, addr) = sock.accept()
              script.log(2, "monThread: accept connection from " + 
                          repr(addr))
              can_read.append(new_conn)

            else:
              message = handle.recv(4096)
              message = message.strip()
              script.log(3, "monThread: message='" + message+"'")
              if (len(message) == 0):
                script.log(2, "monThread: closing connection")
                handle.close()
                for i, x in enumerate(can_read):
                  if (x == handle):
                    del can_read[i]
              else:
                if message == "smrb_status":
                  script.log (3, "monThread: " + str(smrb))
                  handle.send(serialized)
          
      for i, x in enumerate(can_read):
        x.close()
        del can_read[i]

    except:
      self.quit_event.set()
      script.log(1, "monThread: exception caught: " +
                  str(sys.exc_info()[0]))
      print '-'*60
      traceback.print_exc(file=sys.stdout)
      print '-'*60
      for i, x in enumerate(can_read):
        x.close()
        del can_read[i]

  def getDBState (self, key):
    cmd = "dada_dbmetric -k " + key
    rval, lines = system (cmd, False)
    if rval == 0:
      a = lines[0].split(',')
      dat = {'nbufs':a[0], 'full':a[1], 'clear':a[2], 'written':a[3],'read':a[4]}
      hdr = {'nbufs':a[5], 'full':a[6], 'clear':a[7], 'written':a[8],'read':a[9]}
      return 0, hdr, dat
    else:
      return 1, {}, {}



#
class SMRBDaemon(Daemon,StreamBased):

  def __init__ (self, name, id):
    Daemon.__init__(self, name, str(id))
    StreamBased.__init__(self, id, self.cfg)

  def main (self):

    mon_threads = []

    # get a list of data block ids
    db_ids = self.cfg["DATA_BLOCK_IDS"].split(" ")
    db_prefix = self.cfg["DATA_BLOCK_PREFIX"]
    num_stream = self.cfg["NUM_STREAM"]
    numa_node = self.cfg["STREAM_NUMA_" + self.id]
    db_keys = []

    for db_id in db_ids:
      db_key = self.getDBKey (db_prefix, self.id, num_stream, db_id)
      self.log (0, "main: db_key for " + db_id + " is " + db_key)

      nbufs = self.cfg["BLOCK_NBUFS_" + db_id]
      bufsz = self.cfg["BLOCK_BUFSZ_" + db_id]
      nread = self.cfg["BLOCK_NREAD_" + db_id]
      page  = self.cfg["BLOCK_PAGE_" + db_id]

      # check if the datablock already exists
      cmd = "ipcs | grep 0x0000" + db_key + " | wc -l"
      rval, lines = self.system (cmd)
      if rval == 0 and len(lines) == 1 and lines[0] == "1":
        self.log (-1, "Data block with key " + db_key + " existed at launch")
        cmd = "dada_db -k " + db_key + " -d"
        rval, lines = self.system (cmd)
        if rval != 0:
          self.log (-2, "Could not destroy existing datablock")

      cmd = "dada_db -k " + db_key + " -n " + nbufs + " -b " + bufsz + " -r " + nread + " -c " + numa_node 
      if page:
        cmd += " -p -l"
      self.log (1, cmd)
      rval, lines = self.system (cmd)
      db_keys.append(db_key)

    # after creation, launch thread to monitor smrb, maintaining state
    mon_thread = monThread (db_keys, self)
    mon_thread.start()

    while (not self.quit_event.isSet()):
      sleep(1)

    for db_key in db_keys:
      cmd = "dada_db -k " + db_key + " -d"
      rval, lines = self.system (cmd)

    if mon_thread:
      self.log(2, "joining mon thread")
      mon_thread.join()

  @classmethod
  def getDBKey(self, inst_id, stream_id, num_stream, db_id):
    index = (int(db_id) * int(num_stream)) + int(stream_id)
    db_key = inst_id + "%03x" % (2 * index)
    return db_key

  @classmethod
  def getDBMonPort (self, stream_id):
    start_port = 20000
    return start_port + int(stream_id)

if __name__ == "__main__":

  if len(sys.argv) != 2:
    print "ERROR: 1 command line argument expected"
    sys.exit(1)

  # this should come from command line argument
  stream_id = sys.argv[1]

  script = SMRBDaemon ("spip_smrb", stream_id)
  state = script.configure (DAEMONIZE, DL, "smrb", "smrb")
  if state != 0:
    sys.exit(state)

  script.log(1, "STARTING SCRIPT")

  try:

    script.main()

  except:

    script.quit_event.set()

    script.log(-2, "exception caught: " + str(sys.exc_info()[0]))
    print '-'*60
    traceback.print_exc(file=sys.stdout)
    print '-'*60

  script.log(1, "STOPPING SCRIPT")
  script.conclude()
  sys.exit(0)

