#!/usr/bin/env python

###############################################################################
#  
#     Copyright (C) 2015 by Andrew Jameson
#     Licensed under the Academic Free License version 2.1
# 
###############################################################################


import sys, socket, select, traceback, threading, errno, xmltodict
from time import sleep
import abc

from spip.daemons.bases import StreamBased
from spip.daemons.daemon import Daemon
from spip.log_socket import LogSocket
from spip.utils.sockets import getHostNameShort
from spip.config import Config

DAEMONIZE = False
DL = 2

#################################################################
# thread for reading .dada files into a ring buffer
class diskdbThread (threading.Thread):

  def __init__ (self, parent, key, files, pipe):
    threading.Thread.__init__(self)
    self.parent = parent 
    self.key = key
    self.pipe = pipe
    self.files = files

  def run (self):
    cmd = "dada_diskdb -k " + self.key + " -z"
    for file in self.files:
      cmd += " -f " + file
    rval = self.parent.system_piped (cmd, self.pipe)
    return rval

class ReadDaemon(Daemon,StreamBased):
  __metaclass__ = abc.ABCMeta

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

    if DL > 1:
      self.log(1, "NBIT\t"  + fixed_config["NBIT"])
      self.log(1, "NDIM\t"  + fixed_config["NDIM"])
      self.log(1, "NCHAN\t" + fixed_config["NCHAN"])
      self.log(1, "TSAMP\t" + fixed_config["TSAMP"])
      self.log(1, "BW\t"    + fixed_config["BW"])
      self.log(1, "FREQ\t"  + fixed_config["FREQ"])
      self.log(1, "START_CHANNEL\t"  + fixed_config["START_CHANNEL"])
      self.log(1, "END_CHANNEL\t"  + fixed_config["END_CHANNEL"])

    self.log(1, "ReadDaemon::main self.list_obs()")
    list_xml_str = self.list_obs()
    list_xml = xmltodict.parse (list_xml_str)
    first_obs = list_xml['observation_list']['observation'][0]
    print str(first_obs)
    self.read_obs (first_obs)
    #self.log(1, "ReadDaemon::main " + str(xml))

    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind((hostname, int(self.cfg["STREAM_READ_PORT"]) + int(self.id)))
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
            
            xml = xmltodict.parse (message)
            self.log(3, DL, "commandThread: xml='" + str(xml) +"'")

            if (len(message) == 0):
              self.log(1, "commandThread: closing connection")
              handle.close()
              for i, x in enumerate(can_read):
                if (x == handle):
                  del can_read[i]
            else:

              if xml['command'] == "list_obs":
                self.log (1, "command ["+xml['command'] + "]")
                self.list_obs ()
                response = "OK"

              elif xml['command'] == "read_obs":
                self.log (1, "command ["+xml['command'] + "]")
                self.read_obs ()
                response = "OK"

              else:
                self.log (-1, "unrecognized command ["+xml['command'] + "]")
                response = "FAIL"
  
              self.log(3, "-> " + response)
              xml_response = "<read_response>" + response + "</read_response>"
              handle.send (xml_response)

  ###############################################################################
  # look for observations on disk that match
  @abc.abstractmethod
  def list_obs (self): pass

  ###############################################################################
  # look for observations on disk that match
  @abc.abstractmethod
  def read_obs (self, xml): pass

if __name__ == "__main__":

  if len(sys.argv) != 2:
    print "ERROR: 1 command line argument expected"
    sys.exit(1)

  # this should come from command line argument
  stream_id = sys.argv[1]

  script = ReadDaemon ("spip_read", stream_id)
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
