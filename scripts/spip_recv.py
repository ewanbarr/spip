#!/usr/bin/env python

###############################################################################
#  
#     Copyright (C) 2015 by Andrew Jameson
#     Licensed under the Academic Free License version 2.1
# 
###############################################################################

import sys, traceback
from time import sleep
from os import environ

from spip.daemons.bases import StreamBased
from spip.daemons.daemon import Daemon
from spip.threads.reporting_thread import ReportingThread
from spip.log_socket import LogSocket
from spip.utils import sockets
from spip.utils.sockets import getHostNameShort
from spip.config import Config
from spip_smrb import SMRBDaemon

DAEMONIZE = True
DL = 1

###############################################################################
# allows for reconfiguration of the multicast address and port
class ConfiguringThread (ReportingThread):

  def __init__ (self, script, id):
    self.id = id
    self.script = script
    host = getHostNameShort()
    port = int(script.cfg["STREAM_RECV_PORT"]) + int(id)
    self.script.log (0, "ConfiguringThread listening on " + host + ":" + str(port))
    ReportingThread.__init__(self, script, host, port)

  def parse_message (self, request):

    self.script.log (2, "ConfiguringThread:parse_message: " + str(request))

    xml = ""
    req = request["recv_cmd"]

    if req["command"] == "configure":
      self.script.log (0, "ConfiguringThread:parse_message received configure command")
      if self.script.configured:
        self.deconfigure ()
      self.configure (req["params"]["param"])
      return (True, "ok")

    elif req["command"] == "deconfigure":
      self.script.log (0, "ConfiguringThread:parse_message: deconfigure command")
      self.deconfigure ()
      return (True, "ok")

    else:

      self.script.log (0, "ConfiguringThread:parse_message: unrecognised command [" + req["command"] + "]")
      return (True, "fail")

  def configure (self, params):
    self.script.log (1, "ConfiguringThread:configure")
    for param in params:
      key = param["@key"]
      value = param["#text"]
      self.script.log (2, "ConfiguringThread:configure: key=" + str(key) + " value=" + str(value))
      self.script.local_config[key] = value
      self.script.configured = True

  def deconfigure (self):
    self.script.log (1, "ConfiguringThread:deconfigure")
    self.script.configured = False
    if self.script.running:
      for binary in self.script.binary_list:
        cmd = "pkill -f '^" + binary + "'"
        rval, lines = self.script.system (cmd, 1)


class RecvDaemon(Daemon,StreamBased):

  def __init__ (self, name, id):
    Daemon.__init__(self, name, str(id))
    StreamBased.__init__(self, id, self.cfg)

    self.configured = False
    self.running = False
    self.local_config = []

  def main (self):

    self.log(2, "main: self.waitForSMRB()")
    smrb_exists = self.waitForSMRB()

    if not smrb_exists:
      self.log(-2, "smrb["+str(self.id)+"] no valid SMRB with " +
                  "key=" + self.db_key)
      self.quit_event.set()
      return

    # configuration file for recv stream
    self.local_config = self.getConfiguration()
    self.local_config_file = "/tmp/spip_stream_" + str(self.id) + ".cfg"

    self.cpu_core = self.cfg["STREAM_RECV_CORE_" + str(self.id)]
    self.ctrl_port = str(int(self.cfg["STREAM_CTRL_PORT"]) + int(self.id))
  
    self.configured = True
    self.running = False
    env = self.getEnvironment()

    # external control loop to allow for reconfiguration of RECV
    while not self.quit_event.isSet():
    
      self.log(3, "main: waiting for configuration")
      while not self.quit_event.isSet() and not self.configured:
        sleep(1) 
      if self.quit_event.isSet():
        return
      Config.writeDictToCFGFile (self.local_config, self.local_config_file)
      self.log(3, "main: configured")

      cmd = self.getCommand(self.local_config_file)
      self.binary_list.append (cmd)

      self.log(3, "main: sleep(1)")
      sleep(1)

      self.log(3, "main: log_pipe = LogSocket(recv_src)")
      log_pipe = LogSocket ("recv_src", "recv_src", str(self.id), "stream",
                            self.cfg["SERVER_HOST"], self.cfg["SERVER_LOG_PORT"],
                            int(DL))

      self.log(3, "main: log_pipe.connect()")
      log_pipe.connect()

      self.log(3, "main: sleep(1)")
      sleep(1)

      self.running = True

      recv_cmd = "numactl -C 6 -- " + cmd
     
      # this should be a persistent / blocking command 
      rval = self.system_piped (recv_cmd, log_pipe.sock, int(DL), env)

      self.running = False 
      self.binary_list = []

      if rval:
        if self.quit_event.isSet():
          self.log (-2, cmd + " failed with return value " + str(rval))
      log_pipe.close ()


  # wait for the SMRB to be created
  def waitForSMRB (self):

    db_id = self.cfg["PROCESSING_DATA_BLOCK"]
    db_prefix = self.cfg["DATA_BLOCK_PREFIX"]
    num_stream = self.cfg["NUM_STREAM"]
    self.db_key = SMRBDaemon.getDBKey (db_prefix, self.id, num_stream, db_id)

    # port of the SMRB daemon for this stream
    smrb_port = SMRBDaemon.getDBMonPort(self.id)

    # wait up to 30s for the SMRB to be created
    smrb_wait = 60

    smrb_exists = False
    while not smrb_exists and smrb_wait > 0 and not self.quit_event.isSet():

      self.log(2, "trying to open connection to SMRB")
      smrb_sock = sockets.openSocket (DL, "localhost", smrb_port, 1)
      if smrb_sock:
        smrb_sock.send ("smrb_status\r\n")
        junk = smrb_sock.recv (65536)
        smrb_sock.close()
        smrb_exists = True
      else:
        sleep (1)
        smrb_wait -= 1

    return smrb_exists


  # return the configuration
  def getConfiguration (self):
    local_config = self.config.getStreamConfigFixed (self.id)
    return local_config

  def getEnvironment (self):
    return environ.copy()

  def getCommand (self, config_file):

    (stream_ip, stream_port) =  self.cfg["STREAM_UDP_" + str(self.id)].split(":")
    cmd = self.cfg["STREAM_BINARY"] + " -k " + self.db_key \
            + " -v -b " + self.cpu_core \
            + " -c " + self.ctrl_port \
            + " -p " + stream_port \
            + " " + config_file + " " + stream_ip
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

  script = RecvDaemon ("spip_recv", stream_id)
  state = script.configure (DAEMONIZE, DL, "recv", "recv")
  if state != 0:
    sys.exit(state)

  script.log(1, "STARTING SCRIPT")

  try:
 
    configuring_thread = ConfiguringThread (script, stream_id)
    configuring_thread.start()

    script.main ()

    configuring_thread.join()

  except:

    script.quit_event.set()

    script.log(-2, "exception caught: " + str(sys.exc_info()[0]))
    print '-'*60
    traceback.print_exc(file=sys.stdout)
    print '-'*60

  script.log(1, "STOPPING SCRIPT")
  script.conclude()
  sys.exit(0)
