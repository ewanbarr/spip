#!/usr/bin/env python

##############################################################################
#  
#     Copyright (C) 2015 by Andrew Jameson
#     Licensed under the Academic Free License version 2.1
# 
###############################################################################

import socket, sys, traceback
from select import select
from xmltodict import parse

#import os, threading, sys, time, socket, select, signal, traceback, xmltodict
#import spip

from spip import config
from spip.daemons.bases import ServerBased,BeamBased
from spip.daemons.daemon import Daemon
from spip.utils import sockets

#from spip.threads.control_thread import ControlThread

DAEMONIZE = False
DL     = 2

class LogsDaemon(Daemon):

  def __init__ (self, name, id):
    Daemon.__init__(self, name, str(id))

  # over ride the default log message so that 
  def log (self, level, message):

    if level <= DL:
      if self.id == "-1":
        file = self.log_dir + "/spip_logs.log"
      else:
        file = self.log_dir + "/spip_logs_" + str(self.id) + ".log"
      fptr = open(file, 'a')
      fptr.write("logs: " + message + "\n")
      fptr.close()

  # override configure logs too
  def configureLogs (self, source, dest, type):
    self.log (0, "socket logging disabled")

  def main (self):

    log_host = sockets.getHostNameShort()
    log_port = self.cfg["SERVER_LOG_PORT"]

    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    self.log(2, "main: binding to " + log_host + ":" + log_port)
    sock.bind((log_host, int(log_port)))

    # allow up to 10 queued connections
    sock.listen(10)

    can_read = [sock]
    can_write = []
    can_error = []
    headers = {}
    line_buffers = {}
    timeout = 1

    while (not self.quit_event.isSet()):

      self.log(3, "main: calling select len(can_read)="+str(len(can_read)))
      timeout = 1
      did_read, did_write, did_error = select(can_read, can_write, can_error, timeout)
      self.log(3, "main: read="+str(len(did_read))+" write="+ \
                    str(len(did_write))+" error="+str(len(did_error)))

      if (len(did_read) > 0):
        for handle in did_read:
          if (handle == sock):
            (new_conn, addr) = sock.accept()
            self.log(1, "main: accept connection from "+repr(addr))
            can_read.append(new_conn)
      
            # read header information about this logging stream
            header = new_conn.recv (4096)
            # return a single character to confirm header has been receive
            new_conn.send('\n')
            header = header.strip()
           
            xml = parse(header)

            headers[new_conn] = xml
            line_buffers[new_conn] = ""

          else:

            message = handle.recv(4096)

            # if the socket has been closed
            if len(message) == 0:
              self.log(1, "commandThread: closing connection")
              if len(line_buffers[handle]) > 0:
                line = line_buffers[handle]
                self.processLine(line, headers[handle])
              handle.close()
              for i, x in enumerate(can_read):
                if (x == handle):
                  del can_read[i]
              del headers[handle]
              del line_buffers[handle]

            else:
              well_terminated = message.endswith('\n')
              line_buffers[handle] += message.rstrip('\n')
              lines = line_buffers[handle].split("\n")
              to_use = len(lines)

              if well_terminated:
                line_buffers[handle] = ""

              for i in range(to_use):
                line = lines[i]
                self.log(3, "commandThread: line='" + line + "'")
                self.processLine (line, headers[handle])


  def processLine (self, line, header):
    
    source = header['log_stream']['source']
    dest   = header['log_stream']['dest']
    id     = header['log_stream']['id']['#text']
    
    if id == "-1":
      file = self.log_dir + "/spip_" + dest + ".log"
    else:
      file = self.log_dir + "/spip_" + dest + "_" + id + ".log"
    fptr = open(file, 'a')
    fptr.write(source + ": " + line + "\n")
    fptr.close()


class LogsServerDaemon (LogsDaemon, ServerBased):

  def __init__ (self, name):
    LogsDaemon.__init__(self,name, "-1")
    ServerBased.__init__(self, self.cfg)

class LogsBeamDaemon (LogsDaemon, BeamBased):

  def __init__ (self, name, id):
    LogsDaemon.__init__(self, name, str(id))
    BeamBased.__init__(self, str(id), self.cfg)

###############################################################################
#
# spip_logs: main
#
if __name__ == "__main__":

  if len(sys.argv) != 2:
    print "ERROR: 1 command line argument expected"
    sys.exit(1)

  # this should come from command line argument
  beam_id = sys.argv[1]
  
  # if the beam_id is < 0, then there is a single TCS for 
  # all beams, otherwise, 1 per beam
  if int(beam_id) == -1:
    script = LogsServerDaemon ("spip_logs")
  else:
    script = LogsBeamDaemon ("spip_logs", beam_id)

  state = script.configure (DAEMONIZE, DL, "logs", "logs")
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

