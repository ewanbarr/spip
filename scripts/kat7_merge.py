#!/usr/bin/env python

###############################################################################
#  
#     Copyright (C) 2015 by Andrew Jameson
#     Licensed under the Academic Free License version 2.1
# 
###############################################################################

import sys, traceback
from time import sleep

from spip.daemons.bases import StreamBased
from spip.daemons.daemon import Daemon
from spip.log_socket import LogSocket
from spip import Config
from spip_smrb import SMRBDaemon

DAEMONIZE = True
DL = 3

class MergeDaemon(Daemon,StreamBased):

  def __init__ (self, name, id):
    Daemon.__init__(self, name, str(id))
    StreamBased.__init__(self, id, self.cfg)

  def main (self):

    in_ids = self.cfg["RECEIVING_DATA_BLOCKS"].split(" ")
    db_prefix = self.cfg["DATA_BLOCK_PREFIX"]
    num_stream = self.cfg["NUM_STREAM"]
    pola_key = SMRBDaemon.getDBKey (db_prefix, self.id, num_stream, in_ids[0])
    polb_key = SMRBDaemon.getDBKey (db_prefix, self.id, num_stream, in_ids[1])
    out_id = self.cfg["PROCESSING_DATA_BLOCK"]
    out_key = SMRBDaemon.getDBKey (db_prefix, self.id, num_stream, out_id)
    self.log(2, "pola_key="+pola_key+" polb_key="+polb_key+" out_key="+out_key)

    # wait up to 10s for the SMRB to be created
    smrb_wait = 10
    cmd = "dada_dbmetric -k " + out_key
    self.binary_list.append (cmd)

    rval = 1
    while rval and smrb_wait > 0 and not self.quit_event.isSet():
      self.log(2, "MergeDaemon::main smrb_wait="+str(smrb_wait))
      rval, lines = self.system (cmd)
      self.log(2, "MergeDaemon::main rval="+str(rval) + " lines="+str(lines))
      if rval:
        self.log(2, "waiting for SMRB to be created")
        sleep(1)
      smrb_wait  = smrb_wait - 1

    if rval:
      self.log(-2, "smrb["+str(self.id)+"] no valid SMRB with " + "key=" + out_key)
      self.quit_event.set()

    else:

      # get the site configuration, things the backend configuration
      # does not affect
      config = Config()

      # generate the front-end configuration file for this stream
      # the does not change from observation to observation
      local_config = config.getStreamConfigFixed(self.id)

      # write this config to file
      config_file = "/tmp/spip_stream_" + str(self.id) + ".cfg"
      Config.writeDictToCFGFile (local_config, config_file)

      stream_core = self.cfg["STREAM_CORE_" + str(self.id)]  

      # TODO CPU/RAM affinity
      cmd = "dada_dbmergedb -w -s " + pola_key + " " + polb_key + " "  + out_key + " -v"
      self.binary_list.append (cmd)

      self.log(2, "MergeDaemon::main log_pipe = LogSocket(merge_src))")
      log_pipe = LogSocket ("merge_src", "merge_src", str(self.id), "stream",
                            self.cfg["SERVER_HOST"], self.cfg["SERVER_LOG_PORT"],
                            int(DL))

      self.log(2, "MergeDaemon::main log_pipe.connect()")
      log_pipe.connect()

      while not self.quit_event.isSet():

        self.log(2, "MergeDaemon::main sleep(1)")
        sleep(1)
     
        # this should be a persistent / blocking command 
        self.log(2, "MergeDaemon::main " + cmd)
        rval = self.system_piped (cmd, log_pipe.sock)
        if rval:
          self.log (-2, cmd + " failed with return value " + str(rval))
          self.quit_event.set()

      self.log(2, "MergeDaemon::main closing log_pipe")
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

  script = MergeDaemon ("kat7_merge", stream_id)
  state = script.configure (DAEMONIZE, DL, "merge", "merge")
  if state != 0:
    sys.exit(state)

  script.log(1, "STARTING SCRIPT")
  script.log(1, "quit_Event="+str(script.quit_event.isSet()))

  try:
    
    script.main ()

  except:

    script.quit_event.set()

    script.log(-2, "exception caught: " + str(sys.exc_info()[0]))
    print '-'*60
    traceback.print_exc(file=sys.stdout)
    print '-'*60

  script.log(1, "STOPPING SCRIPT")
  script.conclude()
  sys.exit(0)
