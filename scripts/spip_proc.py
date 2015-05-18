#!/usr/bin/env python

##############################################################################
#  
#     Copyright (C) 2015 by Andrew Jameson
#     Licensed under the Academic Free License version 2.1
# 
###############################################################################

import os, threading, sys, time, socket, select, signal, traceback
import spip

SCRIPT = "spip_proc"
DL     = 2

def signal_handler(signal, frame):
  print 'You pressed Ctrl+C!'
  global quit_event
  quit_event.set()

#################################################################
# thread for executing processing commands
class procThread (threading.Thread):

  def __init__ (self, cmd, pipe, dl):
    threading.Thread.__init__(self)
    self.cmd = cmd
    self.pipe = pipe
    self.dl = dl

  def run (self):
    rval = spip.system_piped (self.cmd, self.pipe, self.dl <= DL)
    return rval

#################################################################
# main

def main (argv):

  stream_id = argv[1]

  # read configuration file
  cfg = spip.getConfig()

  control_thread = []

  log_file  = cfg["SERVER_LOG_DIR"] + "/" + SCRIPT + ".log"
  pid_file  = cfg["SERVER_CONTROL_DIR"] + "/" + SCRIPT + ".pid"
  quit_file = cfg["SERVER_CONTROL_DIR"] + "/"  + SCRIPT + ".quit"

  if os.path.exists(quit_file):
    sys.stderr.write("quit file existed at launch: " + quit_file)
    sys.exit(1)

  # become a daemon
  spip.daemonize(pid_file, log_file)

  try:

    spip.logMsg(1, DL, "STARTING SCRIPT")

    quit_event = threading.Event()

    signal.signal(signal.SIGINT, signal_handler)

    # start a control thread to handle quit requests
    control_thread = spip.controlThread(quit_file, pid_file, quit_event, DL)
    control_thread.start()

    # just some defaults for now
    log_server = cfg["LOG_SERVER"]
    log_port   = cfg["LOG_PORT"]

    # get the data block key
    db_id = cfg["PROCESSING_DB_ID"]
    db_prefix = cfg["DATA_BLOCK_PREFIX"]
    num_stream = cfg["NUM_STREAM"]
    db_key = spip_smrb.getDBKey (db_prefix, stream_id, num_stream, db_id)
    spip.logMsg (0, DL, "db_key="+db_key)

    # create dspsr input file for the data block
    db_key_filename = "/tmp/spip_" + db_key + ".info"
    db_key_file = open (db_key_filename, "w")
    db_key_file.write("DADA INFO:\n")
    db_key_file.write("INFO: " db_key + "\n")
    db_key_file.close()

    # create a pipe to the logging server
    log_pipe = spip.openSocket (dl, log_server, log_port, 3)

    while (!quit_event.isSet()):

      cmd = "dada_header -k " + db_key
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

        # output directories
        observation_dir = cfg["CLIENT_RESULTS_DIR"] + "/" + utc_start
        fold_dir = observation_dir + "/fold"
        trans_dir = observation_dir + "/trans"
        search_dir = observation_dir + "/search"

        # create output directories
        for dir in (observation_dir, fold_dir, search_dir, trans_dir):
          if os.path.exists(dir):
            spip.logMsg (0, DL, "WARNING: directory existed: " + dir)
          os.makedirs(dir)
        
        # default processing commands
        fold_cmd = "dada_dbnull -s -k " + db_key
        trans_cmd = "dada_dbnull -s -k " + db_key
        search_cmd = "dada_dbnull -s -k " + db_key

        if header["PERFORM_FOLD"] == 1:
          fold_cmd = "dspsr " + db_key_filename + " -cuda 0"

        if header["PERFORM_TRANS"] == 1:
          trans_cmd = "heimdall -k " + db_key + " -gpu_id 1"

        if header["PERFORM_SEARCH"] == 1 or header["PERFORM_TRANS"] == 1:
          search_cmd = "digifil " + db_key_filename + " -c -B 10 -o " + utc_start + " .fil"

        # since dspsr outputs to the cwd, set process CWD to dspsr's
        os.chdir(fold_dir)

        # setup output pipes
        fold_log_pipe   = spip.openSocket (DL, log_host, log_port, 3)
        trans_log_pipe  = spip.openSocket (DL, log_host, log_port, 3)
        search_log_pipe = spip.openSocket (DL, log_host, log_port, 3)

        # create processing threads
        fold_thread = procThread (fold_cmd, fold_log_pipe, 2)
        trans_thread = procThread (trans_cmd, trans_log_pipe, 2)
        search_thread = procThread (search_cmd, search_log_pipe, 2)

        # start processing threads
        fold_thread.run()
        trans_thread.run()
        search_thread.run()

        # join processing threads
        spip.logMsg (2, DL, "joining fold thread")
        rval = fold_thread.join() 
        spip.logMsg (2, DL, "fold thread joined")
        if rval:
          spip.logMsg (-2, DL, "fold thread failed")
          quit_event.set()

        spip.logMsg (2, DL, "joining trans thread")
        rval = trans_thread.join() 
        spip.logMsg (2, DL, "trans thread joined")
        if rval:
          spip.logMsg (-2, DL, "trans thread failed")
          quit_event.set()

        spip.logMsg (2, DL, "joining search thread")
        rval = search_thread.join() 
        spip.logMsg (2, DL, "search thread joined")
        if rval:
          spip.logMsg (-2, DL, "search thread failed")
          quit_event.set()
        
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
