#!/usr/bin/env python

##############################################################################
#  
#     Copyright (C) 2015 by Andrew Jameson
#     Licensed under the Academic Free License version 2.1
# 
###############################################################################

from os import path
from sys import stderr
from signal import signal, SIGINT
from time import gmtime
#from calendar import timegm
import time, select
import threading, socket, subprocess

import spip
from spip import LogSocket

class controlThread(threading.Thread):

  def __init__(self, script):
    threading.Thread.__init__(self)
    self.script = script 

  def run (self):
    self.script.log (1, "controlThread: starting")
    self.script.log (2, "controlThread: quit_file=" + self.script.quit_file)
    self.script.log (2, "controlThread: pid_file=" + self.script.pid_file)

    while ( (not path.exists(self.script.quit_file)) and \
            (not self.script.quit_event.isSet()) ):
      time.sleep(1)

    # signal binaries to exit
    for binary in self.script.binary_list:
      cmd = "pkill -f '^" + binary + "'"
      rval, lines = self.script.system (cmd, 3)

    self.script.log (2, "controlThread: quit request detected")
    self.script.quit_event.set()
    self.script.log (2, "controlThread: exiting")


class ReportingThread (threading.Thread):

  def __init__ (self, script, host, port):
    threading.Thread.__init__(self)
    self.script = script
    self.host = host
    self.port = port

  def run (self):

    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

    sock.bind((self.host, int(self.port)))
    sock.listen(1)

    can_read = [sock]
    can_write = []
    can_error = []

    while not self.script.quit_event.isSet():

      timeout = 1

      # wait for some activity on the control socket
      self.script.log (3, "reportingThread: select")
      did_read, did_write, did_error = select.select(can_read, can_write, can_error, timeout)
      self.script.log (3, "reportingThread: read="+str(len(did_read))+" write="+
                  str(len(did_write))+" error="+str(len(did_error)))

      if (len(did_read) > 0):
        for handle in did_read:
          if (handle == sock):
            (new_conn, addr) = sock.accept()
            self.script.log (1, "report: accept connection from "+repr(addr))

            # add the accepted connection to can_read
            can_read.append(new_conn)

          # an accepted connection must have generated some data
          else:
            try:
              raw = handle.recv(4096)
              message = raw.strip()

              self.script.log (1, "reportingThread: message='" + message+"'")
              xml = xmltodict.parse(message)
              spip.logMsg(3, DL, "<- " + str(xml))

              reply = self.parse_message (xml)

              handle.send (reply)

            except socket.error as e:
              if e.errno == errno.ECONNRESET:
                self.script.log (1, "reportingThread: closing connection")
                handle.close()
                for i, x in enumerate(can_read):
                  if (x == handle):
                    del can_read[i]
              else:
                raise

  def parse_message(xml):
    return "ok"

# base class
class Daemon:

  def __init__ (self, name, id):

    self.dl = 1

    self.name = name
    self.cfg = spip.getConfig()
    self.id = id
    self.hostname = spip.getHostNameShort()

    self.req_host = ""
    self.beam_id = -1
    self.subband_id = -1

    self.control_thread = []
    self.log_sock = []
    self.binary_list = []

    self.log_dir = self.cfg["SERVER_LOG_DIR"]
    self.control_dir = self.cfg["SERVER_CONTROL_DIR"]

  def configure (self, daemonize, dl, source, dest):

    # set the script debug level
    self.dl = dl

    # check the script is running on the configured host
    if self.req_host != self.hostname:
      stderr.write ("ERROR: script launched on incorrect host")
      return 1

    self.log_file  = self.log_dir + "/" + self.name + ".log"
    self.pid_file  = self.control_dir + "/" + self.name + ".pid"
    self.quit_file = self.control_dir + "/"  + self.name + ".quit"

    if path.exists(self.quit_file):
      stderr.write ("ERROR: quit file existed at launch: " + self.quit_file + "\n")
      return 1

    # optionally daemonize script
    if daemonize: 
      spip.daemonize (self.pid_file, self.log_file)

    # instansiate a threaded event signal
    self.quit_event = threading.Event()

    # install signal handler for SIGINT
    def signal_handler(signal, frame):
      stderr.write ("CTRL + C pressed\n")
      self.quit_event.set()
    signal(SIGINT, signal_handler)

    type = self.getType()
    self.configureLogs (source, dest, type)

    # start a control thread to handle quit requests
    self.control_thread = controlThread(self)
    self.control_thread.start()

    return 0


  def configureLogs (self, source, dest, type):
    host = self.cfg["SERVER_HOST"]
    port = int(self.cfg["SERVER_LOG_PORT"])
    if self.log_sock:
      self.log_sock.close()
    self.log_sock = LogSocket(source, dest, self.id, type, host, port, self.dl)
    self.log_sock.connect()

  def log (self, level, message):
    self.log_sock.log (level, message)

  def conclude (self):
    self.quit_event.set()

    for binary in self.binary_list:
      cmd = "pkill -f '^" + binary + "'"
      rval, lines = self.system (cmd, 3)

    if self.control_thread:
      self.control_thread.join()
    self.log_sock.close ()

  def system (self, command, dl=2):
    lines = []
    return_code = 0

    self.log (dl, "system: " + command)

    # setup the module object
    proc = subprocess.Popen(command,
                            shell=True,
                            stdin=None,
                            stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT)

    # communicate the command   
    (output, junk) = proc.communicate()

    return_code = proc.returncode

    if return_code:
      self.log (0, "spip.system: " + command + " failed")

    # Once you have a valid response, split the return output    
    if output:
      lines = output.rstrip('\n').split('\n')
      if dl <= self.dl or return_code:
        for line in lines:
          self.log (0, "system: " + line)

    return return_code, lines

  def system_piped (self, command, pipe, dl=2):

    return_code = 0

    self.log(dl, "system_pipe: " + command)

    # setup the module object
    proc = subprocess.Popen(command,
                            shell=True,
                            stdin=None,
                            stdout=pipe,
                            stderr=pipe)

    # now wait for the process to complete
    proc.wait ()

    # discard the return code
    return_code = proc.returncode

    if return_code:
      self.log (0, "system_pipe: " + command + " failed")

    return return_code

class StreamDaemon (Daemon):

  def __init__ (self, name, id):
    Daemon.__init__(self, name, id)
    (self.req_host, self.beam_id, self.subband_id) = self.getConfig(id)
    self.name = self.name + "_" + str(id)
    if id >= 0:
      self.log_dir  = self.cfg["CLIENT_LOG_DIR"]
      self.control_dir = self.cfg["CLIENT_CONTROL_DIR"]

  def getConfig (self, id):
    stream_config = self.cfg["STREAM_" + str(id)]
    (host, beam_id, subband_id) = stream_config.split(":")
    return (host, beam_id, subband_id)

  def getType (self):
    return "stream"

class RecvDaemon (Daemon):

  def __init__ (self, name, id):
    Daemon.__init__(self, name, id)
    (self.req_host, self.beam_id) = self.getConfig(id)
    self.subband_id = 1

  def getConfig (self, id):
    stream_config = self.cfg["RECV_" + str(id)]
    (host, beam_id) = stream_config.split(":")
    return (host, beam_id)

  def getType (self):
    return "recv"

class ServerDaemon (Daemon):

  def __init__ (self, name):
    Daemon.__init__(self, name, "-1")

    # check that independent beams is off
    if self.cfg["INDEPENDENT_BEAMS"] == "1":
      raise Exception ("ServerDaemons incompatible with INDEPENDENT_BEAMS")  
    self.req_host = self.cfg["SERVER_HOST"]
    self.log_dir  = self.cfg["SERVER_LOG_DIR"]
    self.control_dir = self.cfg["SERVER_CONTROL_DIR"]

  def getType (self):
    return "serv"

class BeamDaemon (Daemon):

  def __init__ (self, name, id):
    Daemon.__init__(self, name, id)
    (self.req_host, self.beam_id) = self.getConfig(id)
    self.log_dir  = self.cfg["SERVER_LOG_DIR"]
    self.control_dir = self.cfg["SERVER_CONTROL_DIR"]

  def getConfig (self, id):
    stream_config = self.cfg["STREAM_" + str(id)]
    (host, beam_id) = stream_config.split(":")
    return (host, beam_id)

  def getType (self):
    return "beam"


