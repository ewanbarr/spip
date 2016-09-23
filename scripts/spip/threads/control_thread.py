#!/usr/bin/env python

##############################################################################
#  
#     Copyright (C) 2015 by Andrew Jameson
#     Licensed under the Academic Free License version 2.1
# 
###############################################################################

from os import path
from time import sleep
import threading

class ControlThread(threading.Thread):

  def __init__(self, script):
    threading.Thread.__init__(self)
    self.script = script

  def keep_running (self):
    return (not path.exists(self.script.quit_file)) and (not self.script.quit_event.isSet())

  def run (self):
    self.script.log (1, "ControlThread: starting")
    self.script.log (2, "ControlThread: pid_file=" + self.script.pid_file)
    self.script.log (2, "ControlThread: quit_file=" + self.script.quit_file)
    #self.script.log (2, "ControlThread: reload_file=" + self.script.reload_file)

    while (self.keep_running()):
      sleep(1)

    # signal binaries to exit
    for binary in self.script.binary_list:
      cmd = "pkill -f '^" + binary + "'"
      rval, lines = self.script.system (cmd, 3)

    if path.exists(self.script.quit_file):
      self.script.log (2, "ControlThread: quit request detected")
      self.script.quit_event.set()
    #if path.exists(self.script.reload_file):
    #  self.script.log (2, "ControlThread: reload request detected")
    self.script.log (2, "ControlThread: exiting")
