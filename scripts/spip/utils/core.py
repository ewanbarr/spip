#!/usr/bin/env python

##############################################################################
#  
#     Copyright (C) 2015 by Andrew Jameson
#     Licensed under the Academic Free License version 2.1
# 
###############################################################################

from spip.utils.times import getCurrentTimeUS

import os, re, socket, datetime, threading, time, sys, atexit, errno
import subprocess

def logMsg(lvl, dlvl, message):
  message = message.replace("`","'")
  if (lvl <= dlvl):
    time = getCurrentTimeUS()
    if (lvl == -1):
        sys.stderr.write("[" + time + "] WARN " + message + "\n")
    elif (lvl == -2):
        sys.stderr.write("[" + time + "] ERR  " + message + "\n")
    else:
        sys.stderr.write("[" + time + "] " + message + "\n")

def parseHeader(lines):
  header = {}
  for line in lines:
    parts = line.split()
    if len(parts) > 1:
      header[parts[0]] = parts[1]
  return header

# Run a command with no stdin, and return STDOUT+STDERR interleaved
def system (command, log=False):

  lines = []
  return_code = 0

  if log:
    logMsg (0, 0, "spip.system: " + command + " log=" + str(log))

  # setup the module object
  proc = subprocess.Popen(command,
                          shell=True,   
                          stdin=None,
                          stdout=subprocess.PIPE,
                          stderr=subprocess.STDOUT)

  # communicate the command   
  try:
    (output, junk) = proc.communicate()
  except IOError, e:
    if e.errno == errno.EINTR:
      return (-1, "SIGINT")

  return_code = proc.returncode

  if return_code:
    logMsg (0, 0, "spip.system: " + command + " failed")

  # Once you have a valid response, split the return output    
  if output:
    lines = output.rstrip('\n').split('\n')
    if log or return_code:
      for line in lines:
        logMsg (0, 0, "spip.system: " + line)

  return return_code, lines

def system_piped (command, pipe, log, work_dir=None):

  return_code = 0

  if log:
    logMsg (0, 0, "spip.system_pipe: " + command)

  # setup the module object
  proc = subprocess.Popen(command,
                          shell=True,
                          stdin=None,
                          stdout=pipe,
                          stderr=subprocess.STDOUT,
                          cwd=work_dir)

  # now wait for the process to complete
  proc.wait ()

  # discard the return code
  return_code = proc.returncode

  if return_code:
    logMsg (0, 0, "spip.system_pipe: " + command + " failed")

  return return_code
