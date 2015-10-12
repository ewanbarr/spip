#!/usr/bin/env python

###############################################################################
#  
#     Copyright (C) 2015 by Andrew Jameson
#     Licensed under the Academic Free License version 2.1
# 
###############################################################################

#import os, threading, sys, time, socket, select, signal, traceback, xmltodict

import sys, socket, select, traceback, errno
from time import sleep

from spip.daemons.bases import StreamBased
from spip.daemons.daemon import Daemon
from spip.log_socket import LogSocket
from spip.utils.sockets import getHostNameShort
from spip import config

DAEMONIZE = False
DL = 1

class GenDaemon(Daemon,StreamBased):

  def __init__ (self, name, id):
    Daemon.__init__(self, name, str(id))
    StreamBased.__init__(self, id, self.cfg)

  def main (self):

    # open a listening socket to receive the header to use in configuring the
    # the data generator
    hostname = getHostNameShort()

    # get the site configurationa
    site = config.getSiteConfig()

    # prepare header using configuration file parameters
    fixed_config = config.getStreamConfigFixed(site, self.cfg, self.id)

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

            message = handle.recv(4096).strip()
            self.log(3, "commandThread: message='" + str(message) +"'")
            
            #xml = xmltodict.parse (message)
            #self.log(3, DL, "commandThread: xml='" + str(xml) +"'")

            if (len(message) == 0):
              self.log(1, "commandThread: closing connection")
              handle.close()
              for i, x in enumerate(can_read):
                if (x == handle):
                  del can_read[i]
            else:
              #message = message.upper()
              #self.log(3, DL, "<- " + message)

              self.gen_obs (fixed_config, message)
  
              response = "OK"
              self.log(3, "-> " + response)
              xml_response = "<gen_response>" + response + "</gen_response>"
              handle.send (xml_response)

  ###############################################################################
  # Generate the UDP data stream based on parameters in the XML message
  def gen_obs (self, fixed_config, message):

    self.log(1, "gen_obs: " + str(message))
    header = config.readDictFromString(message)

    # generate the header file to be use by GEN_BINARY
    header_file = "/tmp/spip_" + header["UTC_START"] + "." + self.id + ".header"

    header["HDR_VERSION"] = "1.0"

    # include the fixed configuration
    header.update(fixed_config)

    # rate in Gb/s
    transmit_rate = float(header["BYTES_PER_SECOND"]) * 8.0 / 1000000000.0

    transmit_rate /= 4

    self.log(3, "gen_obs: writing header to " + header_file)
    config.writeDictToCFGFile (header, header_file)

    # determine the parameters for GEN_BINARY
    (stream_ip, stream_port) =  (self.cfg["STREAM_UDP_" + str(self.id)]).split(":")

    stream_core = self.cfg["STREAM_GEN_CORE_" + str(self.id)]  

    tobs = "60"
    if header["TOBS"] != "":
      tobs = header["TOBS"]

    cmd = self.cfg["STREAM_GEN_BINARY"] + " -b " + stream_core \
          + " -p " + stream_port \
          + " -r " + str(transmit_rate) \
          + " -t " + tobs \
          + " " + header_file + " " + stream_ip 
    self.binary_list.append (cmd)

    sleep(1)

    log_pipe = LogSocket ("gen_src", "gen_src", str(self.id), "stream",
                        self.cfg["SERVER_HOST"], self.cfg["SERVER_LOG_PORT"],
                        int(DL))
    log_pipe.connect()

    sleep(1)
   
    # this should be a persistent / blocking command 
    self.log(1, "gen_obs: [START] " + cmd)
    rval = self.system_piped (cmd, log_pipe.sock)
    self.log(1, "gen_obs: [END]   " + cmd)

    if rval:
      self.log (-2, cmd + " failed with return value " + str(rval))
      self.quit_event.set()

    log_pipe.close ()



if __name__ == "__main__":

  if len(sys.argv) != 2:
    print "ERROR: 1 command line argument expected"
    sys.exit(1)

  # this should come from command line argument
  stream_id = sys.argv[1]

  script = GenDaemon ("gen", stream_id)
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
