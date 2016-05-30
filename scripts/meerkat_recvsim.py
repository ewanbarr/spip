#!/usr/bin/env python

###############################################################################
#  
#     Copyright (C) 2016 by Andrew Jameson
#     Licensed under the Academic Free License version 2.1
# 
###############################################################################

import sys, traceback
from time import sleep

from spip_recvsim import RecvSimDaemon
from spip.meerkat_config import MeerKATConfig

DAEMONIZE = True
DL = 1

class MeerKATRecvSimDaemon(RecvSimDaemon):

  def __init__ (self, name, id):
    RecvSimDaemon.__init__(self, name, id)

  def getConfiguration (self):
    meerkat_config = MeerKATConfig()
    local_config = meerkat_config.getStreamConfigFixed (self.id)
    return local_config

  def getCommand (self, config_file):
    cmd = self.cfg["STREAM_BINARY"] + " -k " + self.db_key \
            + " -b " + self.cpu_core \
            + " -c " + self.ctrl_port \
            + " " + config_file 
    return cmd

#
# main
###############################################################################

if __name__ == "__main__":

  if len(sys.argv) != 2:
    print "ERROR: 1 command line argument expected"
    sys.exit(1)

  # this should come from command line argument
  stream_id = sys.argv[1]

  script = MeerKATRecvSimDaemon ("meerkat_recvsim", stream_id)
  state = script.configure (DAEMONIZE, DL, "recvsim", "recvsim")
  if state != 0:
    sys.exit(state)

  script.log(2, "STARTING SCRIPT")

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
