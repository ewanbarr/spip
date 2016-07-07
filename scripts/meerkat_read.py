#!/usr/bin/env python

###############################################################################
#  
#     Copyright (C) 2015 by Andrew Jameson
#     Licensed under the Academic Free License version 2.1
# 
###############################################################################


import sys, socket, select, traceback, threading, errno, xmltodict
from time import sleep

from spip.daemons.bases import StreamBased
from spip.daemons.daemon import Daemon
from spip.log_socket import LogSocket
from spip.utils.sockets import getHostNameShort
from spip.config import Config
from spip_smrb import SMRBDaemon

DAEMONIZE = False
DL = 2

class MeerkatReadDaemon(Daemon,StreamBased):

  def __init__ (self, name, id):
    Daemon.__init__(self, name, str(id))
    StreamBased.__init__(self, id, self.cfg)

  def main (self):

    # open a listening socket to receive the data files to read
    hostname = getHostNameShort()

    # get the site configurationa
    config = Config()

    # prepare header using configuration file parameters
    fixed_config = config.getStreamConfigFixed(self.id)

    db_prefix = self.cfg["DATA_BLOCK_PREFIX"]
    db_id = self.cfg["RECEIVING_DATA_BLOCK"]
    num_stream = self.cfg["NUM_STREAM"]
    db_key = SMRBDaemon.getDBKey (db_prefix, stream_id, num_stream, db_id)

    cmd = "dada_diskdb -k " + db_key + " -z -s " + \
          "-f /data/spip/first_light/single_dish/2016-04-28-13:27:30_0000000000000000.000000.dada" + \
          "-f /data/spip/first_light/single_dish/2016-04-28-13:27:30_0000000000000000.000000.dada" + \
          "-f /data/spip/first_light/single_dish/2016-04-28-13:27:30_0000034359738368.000000.dada" + \
          "-f /data/spip/first_light/single_dish/2016-04-28-13:27:30_0000068719476736.000000.dada" + \
          "-f /data/spip/first_light/single_dish/2016-04-28-13:27:30_0000103079215104.000000.dada" + \
          "-f /data/spip/first_light/single_dish/2016-04-28-13:27:30_0000137438953472.000000.dada" + \
          "-f /data/spip/first_light/single_dish/2016-04-28-13:27:30_0000171798691840.000000.dada" + \
          "-f /data/spip/first_light/single_dish/2016-04-28-13:27:30_0000206158430208.000000.dada" + \
          "-f /data/spip/first_light/single_dish/2016-04-28-13:27:30_0000240518168576.000000.dada"

    self.log (0, "cmd=" + cmd)
    (rval, lines) = self.system (cmd)
    self.log (0, "rval=" + str(rval))
    for line in lines:
      self.log (0, line)


if __name__ == "__main__":

  if len(sys.argv) != 2:
    print "ERROR: 1 command line argument expected"
    sys.exit(1)

  # this should come from command line argument
  stream_id = sys.argv[1]

  script = MeerkatReadDaemon ("meerkat_read", stream_id)
  state = script.configure (DAEMONIZE, DL, "read", "read")
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
