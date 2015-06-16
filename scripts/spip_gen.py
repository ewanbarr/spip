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

DAEMONIZE = False
DL = 2

###############################################################################
# main
#
def main (self, id):

  (stream_ip, stream_port) =  self.cfg["STREAM_UDP_" + str(id)].split(":")

  stream_core = self.cfg["STREAM_GEN_CORE_" + str(id)]  

  cmd = self.cfg["STREAM_GEN_BINARY"] + " " + stream_ip+ " -b " \
        + stream_core + " -p " + stream_port
  self.binary_list.append (cmd)

  time.sleep(2)

  log_pipe = LogSocket ("gen_src", "gen_src", str(id), "stream",
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

  script = StreamDaemon ("spip_gen", stream_id)

  script.configure (DAEMONIZE, DL, "gen", "gen")

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
