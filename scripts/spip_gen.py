#!/usr/bin/env python

###############################################################################
#  
#     Copyright (C) 2015 by Andrew Jameson
#     Licensed under the Academic Free License version 2.1
# 
###############################################################################

import sys, socket, select, threading, traceback, errno
from time import sleep

from spip.daemons.bases import StreamBased
from spip.daemons.daemon import Daemon
from spip.log_socket import LogSocket
from spip.utils.sockets import getHostNameShort
from spip.utils.core import system_piped,system
from spip import Config

DAEMONIZE = True
DL = 2

#################################################################
# thread for executing generator
class genThread (threading.Thread):

  def __init__ (self, script, fixed_config, header, dl):

    threading.Thread.__init__(self)
    self.script = script
    self.dl = dl

    script_id = str(self.script.id)

    # generate the header file to be use by GEN_BINARY
    header_file = "/tmp/spip_" + header["UTC_START"] + "." + script_id + ".header"

    header["HDR_VERSION"] = "1.0"

    # include the fixed configuration
    header.update(fixed_config)

    # rate in Gb/s
    transmit_rate = float(header["BYTES_PER_SECOND"]) * 8.0 / 1000000000.0

    # TODO make this native
    transmit_rate /= 2

    self.script.log(2, "genThread: writing header to " + header_file)
    config.writeDictToCFGFile (header, header_file)

    # determine the parameters for GEN_BINARY
    (stream_ip, stream_port) =  (self.script.cfg["STREAM_UDP_" + script_id]).split(":")

    stream_core = self.script.cfg["STREAM_GEN_CORE_" + script_id]

    tobs = "60"
    if header["TOBS"] != "":
      tobs = header["TOBS"]

    self.cmd = self.script.cfg["STREAM_GEN_BINARY"] + " -b " + stream_core \
          + " -p " + stream_port \
          + " -r " + str(transmit_rate) \
          + " -t " + tobs \
          + " " + header_file + " " + stream_ip

    self.script.log (2, "genThread::init cmd=" + self.cmd)

  def run (self):

    self.script.log (2, "genThread::run creating log_pipe")
    log_pipe = LogSocket ("gen_src", "gen_src", str(self.script.id), "stream",
                          self.script.cfg["SERVER_HOST"], self.script.cfg["SERVER_LOG_PORT"],
                          int(self.dl))
    log_pipe.connect ()
    sleep (1)

    self.script.log (2, "genThread::run START")
    self.script.binary_list.append (self.cmd)
    rval = system_piped (self.cmd, log_pipe.sock, self.dl <= DL)
    self.script.log (2, "genThread::run END")

    self.script.binary_list.remove (self.cmd)

    log_pipe.close()
    return rval

#################################################################
# Implementation
class GenDaemon(Daemon,StreamBased):

  def __init__ (self, name, id):
    Daemon.__init__(self, name, str(id))
    StreamBased.__init__(self, id, self.cfg)
    self.gen_thread = []

  def main (self):

    # open a listening socket to receive the header to use in configuring the
    # the data generator
    hostname = getHostNameShort()

    config = Config()

    # prepare header using configuration file parameters
    fixed_config = config.getStreamConfigFixed(self.id)

    if DL > 1:
      self.log(1, "NBIT\t"  + fixed_config["NBIT"])
      self.log(1, "NDIM\t"  + fixed_config["NDIM"])
      self.log(1, "NCHAN\t" + fixed_config["NCHAN"])
      self.log(1, "TSAMP\t" + fixed_config["TSAMP"])
      self.log(1, "BW\t"    + fixed_config["BW"])
      self.log(1, "FREQ\t"  + fixed_config["FREQ"])
      self.log(1, "START_CHANNEL\t"  + fixed_config["START_CHANNEL"])
      self.log(1, "END_CHANNEL\t"  + fixed_config["END_CHANNEL"])

    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind((hostname, int(self.cfg["STREAM_GEN_PORT"]) + int(self.id)))
    sock.listen(1)

    can_read = [sock]
    can_write = []
    can_error = []

    while not self.quit_event.isSet():

      timeout = 1

      did_read = []
      did_write = []
      did_error = []

      try:
        # wait for some activity on the control socket
        self.log(3, "main: select")
        did_read, did_write, did_error = select.select(can_read, can_write, can_error, timeout)
        self.log(3, "main: read="+str(len(did_read))+" write="+
                    str(len(did_write))+" error="+str(len(did_error)))
      except select.error as e:
        if e[0] == errno.EINTR:
          self.log(0, "SIGINT received during select, exiting")
          self.quit_event.set()
        else:
          raise

      if (len(did_read) > 0):
        for handle in did_read:
          if (handle == sock):
            (new_conn, addr) = sock.accept()
            self.log(1, "main: accept connection from "+repr(addr))

            # add the accepted connection to can_read
            can_read.append(new_conn)

          # an accepted connection must have generated some data
          else:

            try:
              message = handle.recv(4096).strip()
              self.log(3, "commandThread: message='" + str(message) +"'")
            
              # and empty messages means a remote close (normally)
              if (len(message) == 0):
                self.log(1, "commandThread: closing connection")
                handle.close()
                for i, x in enumerate(can_read):
                  if (x == handle):
                    del can_read[i]

              # handle the command (non-blocking)
              else:
                self.log (3, "<- " + message)
                self.log (2, "genDaemon::handle_command()")
                (result, response) = self.handle_command (fixed_config, message)
                self.log (2, "genDaemon::handle_command() =" + result)

                self.log (3, "-> " + result + " " + response)
                xml_response = "<gen_response>" + response + "</gen_response>"
                handle.send (xml_response)

            except socket.error as e:
              if e.errno == errno.ECONNRESET:
                self.log(1, "commandThread: closing connection")
                handle.close()
                for i, x in enumerate(can_read):
                  if (x == handle):
                    del can_read[i]

      if self.gen_thread:
        if not self.gen_thread.is_alive():
          # join a previously launched and no longer running gen thread
          rval = self.gen_thread.join()
          if rval:
            self.log (-2, "gen thread failed")
          self.gen_thread = []


  #############################################################################
  # Generate/Cease the UDP data stream based on parameters in the XML message
  def handle_command (self, fixed_config, message):

    self.log(3, "handle_command: " + str(message))
    header = config.readDictFromString(message)

    # If we have a start command
    if header["COMMAND"] == "START":
      self.log(3, "genDaemon::handle_command header[COMMAND]==START")
      if self.gen_thread:
        self.log(-1, "handle_command: received START command whilst already sending")
        return ("fail", "received START whilst sending")

      self.gen_thread = genThread (self, fixed_config, header, DL)
      self.gen_thread.start()

    if header["COMMAND"] == "STOP":
      self.log(3, "genDaemon::handle_command header[COMMAND]==STOP")
      if not self.gen_thread:
        self.log(-1, "handle_command: received STOP command whilst IDLE")
        return ("ok", "received STOP whilst IDLE")

      # signal binary to exit
      for binary in self.binary_list:
        self.log (2, "handle_command: signaling " + binary + " to exit")
        cmd = "pkill -f '^" + binary + "'"
        rval, lines = self.system (cmd, 3)

    return ("ok", "")

if __name__ == "__main__":

  if len(sys.argv) != 2:
    print "ERROR: 1 command line argument expected"
    sys.exit(1)

  # this should come from command line argument
  stream_id = sys.argv[1]

  script = GenDaemon ("spip_gen", stream_id)
  state = script.configure (DAEMONIZE, DL, "gen", "gen")
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
