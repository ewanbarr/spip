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
DL = 1

class RecvSimDaemon(Daemon,StreamBased):

  def __init__ (self, name, id):
    Daemon.__init__(self, name, str(id))
    StreamBased.__init__(self, id, self.cfg)

  def main (self):

    db_id = self.cfg["PROCESSING_DATA_BLOCK"]
    db_prefix = self.cfg["DATA_BLOCK_PREFIX"]
    num_stream = self.cfg["NUM_STREAM"]
    db_key = SMRBDaemon.getDBKey (db_prefix, self.id, num_stream, db_id)
    self.log(0, "db_key="+db_key)

    # wait up to 10s for the SMRB to be created
    smrb_wait = 10
    cmd = "dada_dbmetric -k " + db_key
    self.binary_list.append (cmd)

    rval = 1
    while rval and smrb_wait > 0 and not self.quit_event.isSet():

      rval, lines = self.system (cmd)
      if rval:
        sleep(1)
      smrb_wait -= 1

    if rval:
      self.log(-2, "smrb["+str(self.id)+"] no valid SMRB with " +
                  "key=" + db_key)
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

      ctrl_port = str(int(script.cfg["STREAM_CTRL_PORT"]) + int(self.id))
      cpu_core = self.cfg["STREAM_RECV_CORE_" + str(self.id)]  

      cmd = self.cfg["STREAM_BINARY"] + " -k " + db_key \
            + " -v -b " + cpu_core \
            + " -c " + ctrl_port \
            + " " + config_file
      self.binary_list.append (cmd)

      self.log(3, "main: sleep(1)")
      sleep(1)

      self.log(3, "main: log_pipe = LogSocket(recvsim_src))")
      log_pipe = LogSocket ("recvsim_src", "recvsim_src", str(self.id), "stream",
                            self.cfg["SERVER_HOST"], self.cfg["SERVER_LOG_PORT"],
                            int(DL))

      self.log(3, "main: log_pipe.connect()")
      log_pipe.connect()

      self.log(3, "main: sleep(1)")
      sleep(1)
     
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

  script = RecvSimDaemon ("spip_recvsim", stream_id)
  state = script.configure (DAEMONIZE, DL, "recvsim", "recvsim")
  if state != 0:
    sys.exit(state)

  script.log(1, "STARTING SCRIPT")

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
