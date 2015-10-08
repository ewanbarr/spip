##############################################################################
#  
#     Copyright (C) 2015 by Andrew Jameson
#     Licensed under the Academic Free License version 2.1
# 
###############################################################################

from calendar import timegm
from time import gmtime
from sys import stderr

from spip.utils import times
from spip.utils import sockets

class LogSocket(object):

  def __init__ (self, source, dest, id, type, host, port, dl):
    self.source = source
    self.dest = dest
    self.id = id
    self.host = host
    self.port = port
    self.type = type
    self.dl = dl
    self.last_conn_attempt = 0
    self.log_local = False
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

      self.sock = sockets.openSocket (self.dl, self.host, int(self.port), 1)
      if self.sock:
        self.sock.send (header)
        junk = self.sock.recv(1)
      self.last_conn_attempt = now

  def log (self, level, message):
    if level <= self.dl:
      # if the socket is currently not connected and we didn't try to connect in the last 10 seconds
      if not self.sock:
        self.connect()
      prefix = "[" + times.getCurrentTimeUS() + "] "
      if level == -1:
        prefix += "W "
      if level == -2:
        prefix += "E "
      lines = message.split("\n")
      for line in lines:
        if self.log_local:
          stderr.write (prefix + message + "\n")
        if self.sock:
          self.sock.send (prefix + message + "\n")

  def close (self):
    if self.sock:
      self.sock.close()
    self.sock = []
