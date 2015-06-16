#!/usr/bin/env python

##############################################################################
#  
#     Copyright (C) 2015 by Andrew Jameson
#     Licensed under the Academic Free License version 2.1
# 
###############################################################################

import os, threading, sys, time, socket, select, signal, traceback
import spip
import spip_ct

SCRIPT = "spip_send"
DL     = 2

#################################################################
# main

def main (argv):

  stream_id = argv[1]

  # read configuration file
  cfg = spip.getConfig()

  # get the beam and sub-band identifier
  (req_host, beam_id, subband_id) = spip.getStreamConfig (stream_id, cfg)

  # check if cornerturn is fully contained on single host
  is_local_cornerturn = spip_ct.isLocalSend (stream_id, cfg)

  if (req_host != socket.gethostname().split(".",0))
    sys.stderr.write ("ERROR: script launched on incorrect host")
    sys.exit(1)

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

    # get the data block keys
    db_prefix = cfg["DATA_BLOCK_PREFIX"]
    db_id = cfg["SEND_DB_ID"]
    num_stream = cfg["NUM_STREAM"]
    db_key = spip_smrb.getDBKey (db_prefix, stream_id, num_stream, db_id)

    # get the sender's configration
    (send_beam_id, send_subband_id) = cfg["SEND_" + stream_id].split(":")

    spip.logMsg (0, DL, "db_key=" + db_key_out)

    # create a pipe to the logging server
    log_pipe = spip.openSocket (dl, log_server, log_port, 3)

    prev_utc_start = ""

    while (!quit_event.isSet()):

      cmd = "dada_header -k " + db_key_in
      rval, lines = spip.system (cmd, 2 <= DL)

      # if the command returned ok and we have a header
      if rval != 0:
        spip.logMsg (0, DL, cmd + " failed ")
        quit_event.set()

      else if len(lines) == 0:
      
        spip.logMsg (0, DL, "header was empty"_
        quit_event.set()
      
      else:

        header = spip.parseHeader (lines)
        utc_start = header["UTC_START"]
        if utc_start == prev_utc_start:
          spip.logMsg(-2, DL, "header repeated, exiting")
          quit_event.set()
        else:

          if 

          if send_beam_id == "-" or send_subband_id == "-":
            cmd = "dada_dbull -s -k " + db_key;
          else if is_local_cornerturn:
            cmd = "spip_ct_smrb " + send_id + " -s -k " + db_key + " -c " + spip.getSpipCFGFile()
          else:
            cmd = "spip_ct_send " + send_id + " -s -k " + db_key + " -c " + spip.getSpipCFGFile()

          spip.logMsg(2, DL, cmd)
          rval = spip.system_piped (cmd, pipe, 2 < DL)
          spip.logMsg(2, DL, "rval=" + str(rval))

          prev_utc_start = utc_start

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
