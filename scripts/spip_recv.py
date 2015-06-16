#!/usr/bin/env python

###############################################################################
#  
#     Copyright (C) 2015 by Andrew Jameson
#     Licensed under the Academic Free License version 2.1
# 
###############################################################################

import os, threading, sys, time, socket, select, signal, traceback
import spip
from spip_scripts import StreamDaemon,LogSocket
import spip_smrb

DAEMONIZE = False
DL = 2

###############################################################################
# main
#
def main (self, id):

  db_id = self.cfg["PROCESSING_DATA_BLOCK"]
  db_prefix = self.cfg["DATA_BLOCK_PREFIX"]
  num_stream = self.cfg["NUM_STREAM"]
  db_key = spip_smrb.getDBKey (db_prefix, id, num_stream, db_id)

  self.log(0, "db_key="+db_key)

  # wait up to 10s for the SMRB to be created
  smrb_wait = 10
  cmd = "dada_dbmetric -k " + db_key
  self.binary_list.append (cmd)

  rval = 1
  while rval and smrb_wait > 0:
    rval, lines = self.system (cmd)
    if rval:
      time.sleep(1)
    smrb_wait -= 1

  if rval:
    self.log(-2, "spip_smrb["+str(id)+"] no valid SMRB with " +
                "key=" + db_key)
    self.quit_event.set()

  else:
    ctrl_port = str(int(script.cfg["STREAM_CTRL_PORT"]) + int(id))
    log_port  = str(int(script.cfg["STREAM_LOG_PORT"])  + int(id))
    stream_core = self.cfg["STREAM_CORE_" + str(id)]  
    (stream_ip, stream_port) =  self.cfg["STREAM_UDP_" + str(id)].split(":")

    cmd = self.cfg["STREAM_BINARY"] + " -k " + db_key + " -b " + stream_core \
          + " -c " + ctrl_port + " -i " + stream_ip + " -l " + log_port \
          + " -p " + stream_port
    self.binary_list.append (cmd)

    time.sleep(2)

    log_pipe = LogSocket ("recv_src", "recv_src", str(id), "stream",
                          self.cfg["SERVER_HOST"], self.cfg["SERVER_LOG_PORT"],
                          int(DL))
    log_pipe.connect()

    time.sleep(2)
   
    # this should be a persistent / blocking command 
    rval = self.system_piped (cmd, log_pipe.sock)

    if rval:
      self.log (-2, cmd + " failed with return value " + str(rval))
      self.quit_event.set()

    log_pipe.close ()
#
# main
###############################################################################

if __name__ == "__main__":

  if len(sys.argv) != 2:
    print "ERROR: 1 command line argument expected"
    sys.exit(1)

  # this should come from command line argument
  stream_id = sys.argv[1]

  script = StreamDaemon ("spip_recv", stream_id)

  script.configure (DAEMONIZE, DL, "recv", "recv")

  script.log(1, "STARTING SCRIPT")

  try:
    
    main (script, stream_id)

  except:

    script.quit_event.set()

    script.log(-2, "exception caught: " + str(sys.exc_info()[0]))
    print '-'*60
    traceback.print_exc(file=sys.stdout)
    print '-'*60

  script.log(1, "STOPPING SCRIPT")
  script.conclude()
  sys.exit(0)
