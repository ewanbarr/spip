#!/usr/bin/env python

##############################################################################
#  
#     Copyright (C) 2015 by Andrew Jameson
#     Licensed under the Academic Free License version 2.1
# 
###############################################################################

import os, threading, sys, time, socket, select, signal, traceback
import spip
import spip_smrb
from spip_scripts import StreamDaemon,LogSocket

DAEMONIZE = False
DL        = 2

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

def main (self, id):

  # get the data block keys
  db_prefix = self.cfg["DATA_BLOCK_PREFIX"]
  db_id_in  = self.cfg["PROCESSING_DATA_BLOCK"]
  db_id_out = self.cfg["SEND_DATA_BLOCK"]
  num_stream = self.cfg["NUM_STREAM"]
  db_key_in = spip_smrb.getDBKey (db_prefix, stream_id, num_stream, db_id_in)
  db_key_out = spip_smrb.getDBKey (db_prefix, stream_id, num_stream, db_id_out)

  self.log (0, "db_key_in=" + db_key_in + " db_key_out=" + db_key_out)

  # create dspsr input file for the data block
  db_key_filename = "/tmp/spip_" + db_key_in + ".info"
  db_key_file = open (db_key_filename, "w")
  db_key_file.write("DADA INFO:\n")
  db_key_file.write("INFO: " +  db_key_in + "\n")
  db_key_file.close()

  # create a pipe to the logging server
  log_pipe = LogSocket ("proc_src", "proc_src", str(id), "stream",
                        self.cfg["SERVER_HOST"], self.cfg["SERVER_LOG_PORT"],
                        int(DL))
  log_pipe.connect()
  prev_utc_start = ""

  while (not self.quit_event.isSet()):

    cmd = "dada_header -k " + db_key_in
    self.binary_list.append (cmd)
    rval, lines = self.system (cmd)

    # if the command returned ok and we have a header
    if rval != 0:
      self.log (-2, cmd + " failed")
      self.quit_event.set()

    elif len(lines) == 0:
    
      self.log (-2, "header was empty")
      self.quit_event.set()
    
    else:

      header = spip.parseHeader (lines)

      utc_start = header["UTC_START"]

      # default processing commands
      fold_cmd = "dada_dbnull -s -k " + db_key_in
      trans_cmd = "dada_dbnull -s -k " + db_key_out
      search_cmd = "dada_dbnull -s -k " + db_key_in

      if prev_utc_start == utc_start:
        self.log (-2, "UTC_START [" + utc_start + "] repeated, ignoring observation")
      
      else: 

        # output directories
        observation_dir = self.cfg["CLIENT_RESULTS_DIR"] + "/" + utc_start

        fold_dir = observation_dir + "/fold"
        trans_dir = observation_dir + "/trans"
        search_dir = observation_dir + "/search"

        # if we have multiple sub-bands, transient search will occur after
        # a cornerturn on a different logical processing script
        if int(cfg["NUM_SUBBAND"] ) > 1:
          local_dirs = (observation_dir, fold_dir, search_dir)
        else:
          local_dirs = (observation_dir, fold_dir, trans_dir, search_dir)

        # create output directories
        for dir in local_dirs:
          if os.path.exists(dir):
            self.log (0, "WARNING: directory existed: " + dir)
          os.makedirs(dir)
        
        if header["PERFORM_FOLD"] == 1:
          fold_cmd = "dspsr " + db_key_filename + " -cuda 0"

        if header["PERFORM_SEARCH"] == 1 or header["PERFORM_TRANS"] == 1:
          search_cmd = "digifil " + db_key_filename + " -c -B 10 -o " + utc_start + " .fil"
          if header["PERFORM_TRANS"] == 1:
            search_cmd += " -k " + db_key_out
       
        # if we need to cornerturn output of the search
        if int(cfg["NUM_SUBBAND"] ) == 1:
          trans_cmd = "heimdall -k " + db_key_out + " -gpu_id 1"

        # since dspsr outputs to the cwd, set process CWD to dspsr's
        os.chdir(fold_dir)

      # setup output pipes
      fold_log_pipe   = spip.openSocket (DL, log_host, log_port, 3)
      trans_log_pipe  = spip.openSocket (DL, log_host, log_port, 3)
      search_log_pipe = spip.openSocket (DL, log_host, log_port, 3)

      self.binary_list.append (fold_cmd)
      self.binary_list.append (trans_cmd)
      self.binary_list.append (search_cmd)

      # create processing threads
      fold_thread = procThread (fold_cmd, fold_log_pipe, 2)
      trans_thread = procThread (trans_cmd, trans_log_pipe, 2)
      search_thread = procThread (search_cmd, search_log_pipe, 2)

      # start processing threads
      fold_thread.run()
      trans_thread.run()
      search_thread.run()

      # join processing threads
      self.log (2, "joining fold thread")
      rval = fold_thread.join() 
      self.log (2, "fold thread joined")
      if rval:
        self.log (-2, "fold thread failed")
        quit_event.set()

      self.log (2, "joining trans thread")
      rval = trans_thread.join() 
      self.log (2, "trans thread joined")
      if rval:
        self.log (-2, "trans thread failed")
        quit_event.set()

      self.log (2, "joining search thread")
      rval = search_thread.join() 
      self.log (2, "search thread joined")
      if rval:
        self.log (-2, "search thread failed")
        quit_event.set()
        
###############################################################################

if __name__ == "__main__":

  if len(sys.argv) != 2:
    print "ERROR: 1 command line argument expected"
    sys.exit(1)

  # this should come from command line argument
  stream_id = sys.argv[1]

  script = StreamDaemon ("spip_proc", stream_id)

  rval = script.configure (DAEMONIZE, DL, "proc", "proc") 
  if rval:
    print "ERROR: failed to start"
    sys.exit(1)

  script.log(1, "STARTING SCRIPT")

  try:

    main (script, stream_id)

  except:


    script.log(-2, "exception caught: " + str(sys.exc_info()[0]))
    print '-'*60
    traceback.print_exc(file=sys.stdout)
    print '-'*60
    script.quit_event.set()

  script.log(1, "STOPPING SCRIPT")
  script.conclude()
  sys.exit(0)

