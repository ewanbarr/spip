#!/usr/bin/env python

##############################################################################
#  
#     Copyright (C) 2015 by Andrew Jameson
#     Licensed under the Academic Free License version 2.1
# 
###############################################################################

import os, threading, sys, time, socket, select, traceback

from spip.daemons.bases import StreamBased
from spip.daemons.daemon import Daemon
from spip.log_socket import LogSocket
from spip.config import Config
from spip_smrb import SMRBDaemon
from spip.utils.core import system_piped,system
from spip.utils.sockets import getHostNameShort
from spip.threads.reporting_thread import ReportingThread

DAEMONIZE = True
DL        = 2

#################################################################
# thread for executing processing commands
class procThread (threading.Thread):

  def __init__ (self, cmd, dir, pipe, dl):
    threading.Thread.__init__(self)
    self.cmd = cmd
    self.pipe = pipe
    self.dir = dir  
    self.dl = dl

  def run (self):
    cmd = "cd " + self.dir + "; " + self.cmd
    rval = system_piped (cmd, self.pipe, self.dl <= DL)
    
    if rval == 0:
      rval2, lines = system ("touch " + self.dir + "/obs.finished")
    else:
      rval2, lines = system ("touch " + self.dir + "/obs.failed")

    return rval

###############################################################
# thread for reporting state of spip_proc
class ProcReportingThread (ReportingThread):

  def __init__ (self, script, id):
    host = getHostNameShort()
    port = int(script.cfg["STREAM_PROC_PORT"]) + int(id)
    script.log (0, "ProcReportingThread: listening on " + host + ":" + str(port))
    ReportingThread.__init__(self, script, host, port)

  def parse_message (self, xml):
    self.script.log (0, "parse_message: " + str(xml))
    return True, "ok\r\n"

###############################################################################
# Proc Daemon Proper
class ProcDaemon (Daemon, StreamBased):

  def __init__ (self, name, id):
    Daemon.__init__(self, name, str(id))
    StreamBased.__init__(self, id, self.cfg)

  def main (self):

    stream_id = self.id

    # get the data block keys
    db_prefix = self.cfg["DATA_BLOCK_PREFIX"]
    db_id_in  = self.cfg["PROCESSING_DATA_BLOCK"]
    db_id_out = self.cfg["SEND_DATA_BLOCK"]
    num_stream = self.cfg["NUM_STREAM"]
    cpu_core = self.cfg["STREAM_PROC_CORE_" + stream_id]

    db_key_in = SMRBDaemon.getDBKey (db_prefix, stream_id, num_stream, db_id_in)
    db_key_out = SMRBDaemon.getDBKey (db_prefix, stream_id, num_stream, db_id_out)

    self.log (0, "db_key_in=" + db_key_in + " db_key_out=" + db_key_out)

    # create dspsr input file for the data block
    db_key_filename = "/tmp/spip_" + db_key_in + ".info"
    db_key_file = open (db_key_filename, "w")
    db_key_file.write("DADA INFO:\n")
    db_key_file.write("key " +  db_key_in + "\n")
    db_key_file.close()

    gpu_id = self.cfg["GPU_ID_" + str(self.id)]
    prev_utc_start = ""

    (host, beam, subband) = self.cfg["STREAM_" + stream_id].split(":")

    (cfreq, bw, nchan) = self.cfg["SUBBAND_CONFIG_" + subband].split(":")

    # wait up to 10s for the SMRB to be created
    smrb_wait = 10
    cmd = "dada_dbmetric -k " + db_key_in
    self.binary_list.append (cmd)

    rval = 1
    while rval and smrb_wait > 0 and not self.quit_event.isSet():

      rval, lines = self.system (cmd)
      if rval:
        time.sleep(1)
      smrb_wait -= 1

    if rval:
      self.log(-2, "smrb["+str(self.id)+"] no valid SMRB with " +
                  "key=" + db_key_in)
      self.quit_event.set()

    else:

      while (not self.quit_event.isSet()):

        cmd = "dada_header -k " + db_key_in
        self.binary_list.append (cmd)
        self.log(0, cmd)
        rval, lines = self.system (cmd)

        # if the command returned ok and we have a header
        if rval != 0:
          if self.quit_event.isSet():
            self.log (2, cmd + " failed, but quit_event true")
          else:
            self.log (-2, cmd + " failed")
            self.quit_event.set()

        elif len(lines) == 0:
        
          self.log (-2, "header was empty")
          self.quit_event.set()
        
        else:

          header = Config.parseHeader (lines)

          utc_start = header["UTC_START"]
          self.log (1, "UTC_START=" + header["UTC_START"])
          self.log (1, "RESOLUTION=" + header["RESOLUTION"])

          # default processing commands
          fold_cmd = "dada_dbnull -s -k " + db_key_in
          trans_cmd = "dada_dbnull -s -k " + db_key_out
          search_cmd = "dada_dbnull -s -k " + db_key_in

          if prev_utc_start == utc_start:
            self.log (-2, "UTC_START [" + utc_start + "] repeated, ignoring observation")
          
          else: 
            beam = self.cfg["BEAM_" + str(self.beam_id)]

            if not float(bw) == float(header["BW"]):
              self.log (-1, "configured bandwidth ["+bw+"] != header["+header["BW"]+"]")
            if not float(cfreq) == float(header["FREQ"]):
              self.log (-1, "configured cfreq ["+cfreq+"] != header["+header["FREQ"]+"]")
            if not int(nchan) == int(header["NCHAN"]):
              self.log (-2, "configured nchan ["+nchan+"] != header["+header["NCHAN"]+"]")

            source = header["SOURCE"]

            # output directories 
            suffix     = "/processing/" + beam + "/" + utc_start + "/" + source + "/" + cfreq
            fold_dir   = self.cfg["CLIENT_FOLD_DIR"]   + suffix
            trans_dir  = self.cfg["CLIENT_TRANS_DIR"]  + suffix
            search_dir = self.cfg["CLIENT_SEARCH_DIR"] + suffix
            
            fold = False
            search = False
            trans = False 
          
            try:
              fold = (header["PERFORM_FOLD"] == "1")
              search = (header["PERFORM_SEARCH"] == "1")
              trans = (header["PERFORM_TRANS"] == "1")
            except KeyError as e:
              fold = True
              search = False
              trans = False 

            if fold:
              os.makedirs (fold_dir, 0755)
              fold_cmd = "dspsr -Q " + db_key_filename + " -cuda " + gpu_id + " -overlap -minram 4000 -x 16384 -b 1024 -L 5 -no_dyn"
              fold_cmd = "dspsr -Q " + db_key_filename + " -cuda " + gpu_id + " -D 0 -minram 512 -b 1024 -L 10 -no_dyn -skz -skzs 4 -skzm 128 -skz_no_tscr -skz_no_fscr"
              fold_cmd = "dspsr -Q " + db_key_filename + " -cuda " + gpu_id + " -minram 2048 -x 1024 -b 1024 -L 10 -no_dyn"
              fold_cmd = "dspsr -Q " + db_key_filename + " -cuda " + gpu_id + " -D 0 -minram 2048 -b 1024 -L 10 -no_dyn"
              #fold_cmd = "dada_dbdisk -k " + db_key_in + " -s -D " + fold_dir

              header_file = fold_dir + "/obs.header"
              Config.writeDictToCFGFile (header, header_file)

            if search or trans:
              os.makedirs (search_dir, 0755)
              search_cmd = "digifil " + db_key_filename + " -c -B 10 -o " + utc_start + " .fil"
              if trans:
                search_cmd += " -k " + db_key_out

            if trans and int(self.cfg["NUM_SUBBAND"] ) == "1":
              os.makedirs (trans_dir, 0755)
              trans_cmd = "heimdall -k " + db_key_out + " -gpu_id 1"

          log_host = self.cfg["SERVER_HOST"]
          log_port = int(self.cfg["SERVER_LOG_PORT"])

          # setup output pipes
          fold_log_pipe = LogSocket ("fold_src", "fold_src", str(self.id), "stream",
                                       log_host, log_port, int(DL))

          #trans_log_pipe  = LogSocket ("trans_src", "trans_src", str(self.id), "stream",
          #                             log_host, log_port, int(DL))
          #search_log_pipe = LogSocket ("search_src", "search_src", str(self.id), "stream",
          #                             log_host, log_port, int(DL))

          fold_log_pipe.connect()

          self.binary_list.append (fold_cmd)
          #self.binary_list.append (trans_cmd)
          #self.binary_list.append (search_cmd)

          # create processing threads
          self.log (1, "creating processing threads")      
          fold_cmd = "numactl -C " + cpu_core + " -- " + fold_cmd
          fold_thread = procThread (fold_cmd, fold_dir, fold_log_pipe.sock, 1)

          #trans_thread = procThread (trans_cmd, self.log_sock.sock, 2)
          #search_thread = procThread (search_cmd, self.log_sock.sock, 2)

          # start processing threads
          self.log (1, "starting processing threads")      
          fold_thread.start()
          #trans_thread.start()
          #search_thread.start()

          # join processing threads
          self.log (2, "waiting for fold thread to terminate")
          rval = fold_thread.join() 
          self.log (2, "fold thread joined")
          if rval:
            self.log (-2, "fold thread failed")
            quit_event.set()

          #self.log (2, "joining trans thread")
          #rval = trans_thread.join() 
          #self.log (2, "trans thread joined")
          #if rval:
          #  self.log (-2, "trans thread failed")
          #  quit_event.set()

          #self.log (2, "joining search thread")
          #rval = search_thread.join() 
          #self.log (2, "search thread joined")
          #if rval:
          #  self.log (-2, "search thread failed")
          #  quit_event.set()

          fold_log_pipe.close()
          #trans_log_pipe.close()
          #search_log_pipe.close()

        self.log (1, "processing completed")

###############################################################################

if __name__ == "__main__":

  if len(sys.argv) != 2:
    print "ERROR: 1 command line argument expected"
    sys.exit(1)

  # this should come from command line argument
  stream_id = sys.argv[1]

  script = ProcDaemon ("spip_proc", stream_id)

  state = script.configure (DAEMONIZE, DL, "proc", "proc") 
  if state != 0:
    sys.exit(state)

  try:

    reporting_thread = ProcReportingThread (script, stream_id)
    reporting_thread.start()

    script.main ()

    reporting_thread.join()

  except:


    script.log(-2, "exception caught: " + str(sys.exc_info()[0]))
    print '-'*60
    traceback.print_exc(file=sys.stdout)
    print '-'*60
    script.quit_event.set()

  script.conclude()
  sys.exit(0)

