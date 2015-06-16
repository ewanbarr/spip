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
from calendar import timegm
import time
import threading, socket, subprocess

import spip

class LogSocket:

  def __init__ (self, source, dest, id, type, host, port, dl):
    self.source = source
    self.dest = dest
    self.id = id
    self.host = host
    self.port = port
    self.type = type
    self.dl = dl
    self.last_conn_attempt = 0
    self.sock = []

  def connect (self):
    now = timegm(gmtime())
    if (now - self.last_conn_attempt) > 5:
      header = "<?xml version='1.0' encoding='ISO-8859-1'?>" + \
               "<log_stream>" + \
               "<host>" + self.host + "</host>" + \
               "<source>" + self.source + "</source>" + \
               "<dest>" + self.dest + "</dest>" + \
               "<id type='" + self.type + "'>" + self.id + "</id>" + \
              "</log_stream>"

      self.sock = spip.openSocket (self.dl, self.host, int(self.port), 1)
      if self.sock:
        self.sock.send (header)
        junk = self.sock.recv(1)
      self.last_conn_attempt = now

  def log (self, level, message):
    if level <= self.dl:
      # if the socket is currently not connected and we didn't try to connect in the last 10 seconds
      if not self.sock:
        self.connect()
      prefix = "[" + spip.getCurrentSpipTimeUS() + "] "
      if level == -1:
        prefix += "W "
      if level == -2:
        prefix += "E "
      lines = message.split("\n")
      for line in lines:
        stderr.write (prefix + message + "\n")
        if self.sock:
          self.sock.send (prefix + message + "\n")

  def close (self):
    if self.sock:
      self.sock.close()
    self.sock = []

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

  def configure (self, daemonize, dl, source, dest):

    # set the script debug level
    self.dl = dl

    # check the script is running on the configured host
    if self.req_host != self.hostname:
      stderr.write ("ERROR: script launched on incorrect host")
      return 1

    self.log_file  = self.cfg["SERVER_LOG_DIR"] + "/" + self.name + ".log"
    self.pid_file  = self.cfg["SERVER_CONTROL_DIR"] + "/" + self.name + ".pid"
    self.quit_file = self.cfg["SERVER_CONTROL_DIR"] + "/"  + self.name + ".quit"

    if path.exists(self.quit_file):
      stderr.write ("ERROR: quit file existed at launch: " + self.quit_file)
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
                            stderr=subprocess.STDOUT)

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
