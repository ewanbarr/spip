#!/usr/bin/env python

##############################################################################
#  
#     Copyright (C) 2015 by Andrew Jameson
#     Licensed under the Academic Free License version 2.1
# 
###############################################################################

import os, threading, sys, time, socket, select, signal, traceback
import spip

SCRIPT = "spip_recv"
DL     = 2

#################################################################
# main

def main (argv):

  recv_id = argv[1]

  # read configuration file
  config = Config()

  # get the beam identifier
  (req_host, beam_id) = spip.getRecvConfig(recv_id, cfg)

  hostname = spip.getHostNameShort()

  if req_host != hostname:
    print "req_host["+req_host + "] != hostname["+hostname+"]\n"
    sys.stderr.write ("ERROR: script launched on incorrect host\n")
    sys.exit(1)

  # check if cornerturn is fully contained on single host
  is_local_cornerturn = spip_ct.isLocalRecv (recv_id, cfg)

  control_thread = []

  log_file  = cfg["SERVER_LOG_DIR"] + "/" + SCRIPT + ".log"
  pid_file  = cfg["SERVER_CONTROL_DIR"] + "/" + SCRIPT + ".pid"
  quit_file = cfg["SERVER_CONTROL_DIR"] + "/"  + SCRIPT + ".quit"

  if os.path.exists(quit_file):
    sys.stderr.write("ERROR: quit file existed at launch: " + quit_file)
    sys.exit(1)

  # become a daemon
  spip.daemonize(pid_file, log_file)

  try:

    spip.logMsg(1, DL, "STARTING SCRIPT")

    quit_event = threading.Event()

    def signal_handler(signal, frame):
      quit_event.set()

    signal.signal(signal.SIGINT, signal_handler)

    # start a control thread to handle quit requests
    control_thread = spip.controlThread(quit_file, pid_file, quit_event, DL)
    control_thread.start()

    # just some defaults for now
    log_server = cfg["LOG_SERVER"]
    log_port   = cfg["LOG_PORT"]

    # get the output data block key
    db_prefix = cfg["DATA_BLOCK_PREFIX"]
    db_id = cfg["RECV_DATA_BLOCK"]
    num_stream = cfg["NUM_STREAM"]
    db_key = spip_smrb.getDBKey (db_prefix, recv_id, num_stream, db_id)

    # get the receivers's configration
    (recv_host, recv_beam) = cfg["RECV_" + recv_id].split(":")

    spip.logMsg (0, DL, "db_key=" + db_key)

    # create a pipe to the logging server
    log_pipe = spip.openSocket (dl, log_server, log_port, 3)

    while (not quit_event.isSet()):

      if is_local_cornerturn:
        cmd = "spip_ct_smrb " + recv_id + " -s -k " + db_key + " -c " + spip.getSpipCFGFile()
      else:
        cmd = "spip_ct_recv " + recv_id + " -s -k " + db_key + " -c " + spip.getSpipCFGFile()

      spip.logMsg(2, DL, cmd)
      rval = spip.system_piped (cmd, pipe, 2 < DL)
      spip.logMsg(2, DL, "rval=" + str(rval))

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

  spip.logMsg(1, DL, "STOPPING SCRIPT")

  sys.exit(0)

###############################################################################

if __name__ == "__main__":
  if len(sys.argv) != 2:
    print "ERROR: 1 command line argument expected"
    sys.exit(1)

  main (sys.argv)
  sys.exit(0)
