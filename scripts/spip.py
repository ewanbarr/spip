#!/usr/bin/env python

#
# SPIP python module
#

import os, re, socket, datetime, threading, time, sys, atexit, errno
import subprocess

SPIP_ROOT = os.environ.get('SPIP_ROOT');

def getSPIP_ROOT():
  return SPIP_ROOT

def getConfig():
  config_file = getSpipCFGFile()
  config = readCFGFileIntoDict(config_file)
  return config

def getSpipCFGFile():
  return SPIP_ROOT + "/share/spip.cfg"

def readCFGFileIntoDict(filename):
  config = {}
  try:
    fptr = open(filename, 'r')
  except IOError:
    print "ERROR: cannot open " + filename
  else:
    for line in fptr:
      # remove all comments 
      line = line.strip()
      line = re.sub("#.*", "", line);
      if line: 
        line = re.sub("\s+", " ", line)
        parts = line.split(' ', 1)
        if (len(parts) == 2):
          config[parts[0]] = parts[1].strip()
    fptr.closed
    return config

def getHostMachineName():
  fqdn = socket.gethostname()
  parts = fqdn.split('.',1)
  if (len(parts) >= 1):
    host = parts[0]
  if (len(parts) == 2):
    domain = parts[1]
  return host


def logMsg(lvl, dlvl, message):
  message = message.replace("`","'")
  if (lvl <= dlvl):
    time = getCurrentSpipTimeUS()
    if (lvl == -1):
        sys.stderr.write("[" + time + "] WARN " + message + "\n")
    elif (lvl == -2):
        sys.stderr.write("[" + time + "] ERR  " + message + "\n")
    else:
        sys.stderr.write("[" + time + "] " + message + "\n")

def getCurrentSpipTimeUS():
  now = datetime.datetime.today()
  now_str = now.strftime("%Y-%m-%d-%H:%M:%S.%f")
  return now_str

def getCurrentSpipTime(toadd=0):
  now = datetime.datetime.today()
  if (toadd > 0):
    delta = datetime.timedelta(0, toadd)
    now = now + delta
  now_str = now.strftime("%Y-%m-%d-%H:%M:%S")
  return now_str

def getUTCSpipTime(toadd=0):
  now = datetime.datetime.utcnow()
  if (toadd > 0):
    delta = datetime.timedelta(0, toadd)
    now = now + delta
  now_str = now.strftime("%Y-%m-%d-%H:%M:%S")
  return now_str

def parseHeader(lines):
  header = {}
  for line in lines:
    parts = line.split()
    if len(parts) > 1:
      header[parts[0]] = parts[1]
  return header

def getStreamConfig (stream_id, cfg):
  stream_config = cfg["STREAM_" + str(stream_id)]
  (host, beam_id, subband_id) = stream_config.split(":")
  return (host, beam_id, subband_id)

def getHostNameShort ():
  return socket.gethostname().split(".", 1)[0]

#
# Run a command with no stdin, and return STDOUT+STDERR interleaved
#
def system (command, log):

  lines = []
  return_code = 0

  if log:
    logMsg (0, 0, "spip.system: " + command)

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
    logMsg (0, 0, "spip.system: " + command + " failed")

  # Once you have a valid response, split the return output    
  if output:
    lines = output.rstrip('\n').split('\n')
    if log or return_code:
      for line in lines:
        logMsg (0, 0, "spip.system: " + line)

  return return_code, lines

def system_piped (command, pipe, log):

  return_code = 0

  if log:
    logMsg (0, 0, "spip.system_pipe: " + command)

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
    logMsg (0, 0, "spip.system_pipe: " + command + " failed")

  return return_code

###############################################################################
#
# open a standard socket
#
def openSocket(dl, host, port, attempts=10):

  sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
  connected = False

  while (not connected and attempts > 0):

    logMsg(3, dl, "openSocket: attempt " + str(11-attempts))

    try:
      sock.connect((host, port))

    except socket.error, e:
      if e.errno == errno.ECONNREFUSED:
        logMsg(-1, dl, "openSocket: connection to " + host + ":" + str(port) + " refused")
        attempts -= 1
        if  attempts > 0:
          time.sleep(1)
      else:
        raise
    else:
      logMsg(3, dl, "openSocket: conncected")
      connected = True
  
  if connected:
    return sock
  else:  
    sock.close()
    return 0

###############################################################################
#
# Send a message on the socket and read the reponse
# until an ok or fail is read
#
def sendTelnetCommand(sock, msg, timeout=1):
  result = ""
  response = ""
  eod = 0

  sock.send(msg + "\r\n")
  while (not eod):
    reply = sock.recv(4096)
    if (len(reply) == 0):
      eod = 1
    else:
      # remove trailing newlines
      reply = reply.rstrip()
      lines = reply.split("\n")
      for line in lines:
        if ((line == "ok") or (line == "fail")):
          result = reply
          eod = 1
        else:
          if (response == ""):
            response = line
          else:
            response = response + "\n" + line 
 
  return (result, response)

###############################################################################
#
# Turn the calling process into a daemon
#
def daemonize(pidfile, logfile):

  # standard input will always be directed to /dev/null
  stdin = "/dev/null"
  stdout = logfile
  stderr = logfile

  try:
    pid = os.fork()
    if pid > 0:
      # exit first parent
      sys.exit(0)
  except OSError, e:
    sys.stderr.write("fork #1 failed: %d (%s)\n" % (e.errno, e.strerror))
    sys.exit(1)

  # decouple from parent environment
  os.chdir("/")
  os.setsid()
  os.umask(0)

  # do second fork
  try:
    pid = os.fork()
    if pid > 0:
      # exit from second parent
      sys.exit(0)
  except OSError, e:
    sys.stderr.write("fork #2 failed: %d (%s)\n" % (e.errno, e.strerror))
    sys.exit(1)

  # redirect standard file descriptors
  sys.stdout.flush()
  sys.stderr.flush()
  si = file(stdin, 'r')
  so = file(stdout, 'a+')
  se = file(stderr, 'a+', 0)
  os.dup2(si.fileno(), sys.stdin.fileno())
  os.dup2(so.fileno(), sys.stdout.fileno())
  os.dup2(se.fileno(), sys.stderr.fileno())

  # write pidfile, enable a function to cleanup pid file upon crash
  atexit.register(delpid, pidfile)
  pid = str(os.getpid())
  file(pidfile,'w+').write("%s\n" % pid)


###############################################################################
#
# delete pidfile, used for daemonize
#
def delpid(pidfile):
  os.remove(pidfile)

###############################################################################
#
# threading implementation for control thread
#
class controlThread(threading.Thread):

  def __init__(self, quit_file, pid_file, quit_event, dl):
    threading.Thread.__init__(self)
    self.quit_file = quit_file 
    self.pid_file = pid_file 
    self.quit_event = quit_event
    self.dl = dl

  def run(self):
    logMsg(1, self.dl, "controlThread: starting")
    logMsg(2, self.dl, "controlThread: quit_file=" + self.quit_file)
    logMsg(2, self.dl, "controlThread: pid_file=" + self.pid_file)

    while ( (not os.path.exists(self.quit_file)) and \
            (not self.quit_event.isSet()) ):
      time.sleep(1)

    logMsg(2, self.dl, "controlThread: quit request detected") 
    self.quit_event.set()
    logMsg(1, self.dl, "controlThread: exiting")

