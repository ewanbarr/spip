#!/usr/bin/env python

##############################################################################
#  
#     Copyright (C) 2015 by Andrew Jameson
#     Licensed under the Academic Free License version 2.1
# 
###############################################################################

from signal import signal, SIGINT
from time import gmtime
#from calendar import timegm
from atexit import register
import time, select, errno, os, sys
import threading, socket, subprocess

from spip.config import Config
from spip.utils import sockets
from spip.log_socket import LogSocket
from spip.threads.control_thread import ControlThread

# base class
class Daemon(object):

  def __init__ (self, name, id):

    self.dl = 1

    self.name = name
    self.config = Config()
    self.cfg = self.config.getConfig()
    self.id = id
    self.hostname = sockets.getHostNameShort()

    self.req_host = ""
    self.beam_id = -1
    self.subband_id = -1

    self.control_thread = []
    self.log_sock = []
    self.binary_list = []

    self.log_dir = self.cfg["SERVER_LOG_DIR"]
    self.control_dir = self.cfg["SERVER_CONTROL_DIR"]

    # append the streamid/beamid/hostname
    if self.id != -1:
      self.name += "_" + str (self.id)

  def configure (self, become_daemon, dl, source, dest):

    # set the script debug level
    self.dl = dl

    # check the script is running on the configured host
    if self.req_host != self.hostname:
      sys.stderr.write ("ERROR: script requires " + self.req_host +", but was launched on " + self.hostname + "\n")
      return 1

    self.log_file  = self.log_dir + "/" + self.name + ".log"
    self.pid_file  = self.control_dir + "/" + self.name + ".pid"
    self.quit_file = self.control_dir + "/"  + self.name + ".quit"
    #self.reload_file = self.control_dir + "/"  + self.name + ".reload"

    if os.path.exists(self.quit_file):
      sys.stderr.write ("ERROR: quit file existed at launch: " + self.quit_file + "\n")
      return 1

    #if os.path.exists(self.reload_file):
    #  sys.stderr.write ("WARNING: reload file existed at launch: " + self.reload_file + "\n")
    #  os.remove (self.reload_file)

    # optionally daemonize script
    if become_daemon: 
      self.daemonize ()

    # instansiate a threaded event signal
    self.quit_event = threading.Event()

    # install signal handler for SIGINT
    def signal_handler(signal, frame):
      sys.stderr.write ("CTRL + C pressed\n")
      self.quit_event.set()

    signal(SIGINT, signal_handler)

    type = self.getBasis()
    self.configureLogs (source, dest, type)

    # start a control thread to handle quit requests
    self.control_thread = ControlThread(self)
    self.control_thread.start()

    self.log (3, "configure: log_file=" + self.log_file)
    self.log (3, "configure: pid_file=" + self.pid_file)
    self.log (3, "configure: quit_file=" + self.quit_file)
    #self.log (3, "configure: reload_file=" + self.reload_file)

    return 0

  def daemonize(self):

    # standard input will always be directed to /dev/null
    stdin = "/dev/null"
    stdout = self.log_file
    stderr = self.log_file

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
    register(self.delpid)
    pid = str(os.getpid())
    file(self.pid_file,'w+').write("%s\n" % pid)

  def delpid(self):
    if os.path.exists(self.pid_file):
      os.remove(self.pid_file)

  def configureLogs (self, source, dest, type):
    host = self.cfg["SERVER_HOST"]
    port = int(self.cfg["SERVER_LOG_PORT"])
    if self.log_sock:
      self.log_sock.close()
    self.log_sock = LogSocket(source, dest, self.id, type, host, port, self.dl)
    self.log_sock.connect(5)

  def log (self, level, message):
    if self.log_sock:
      if not self.log_sock.connected:
        self.log_sock.connect(1)
      self.log_sock.log (level, message)


  def tryKill (self, signal):
    existed = False
    for binary in self.binary_list:
      cmd = "pgrep -f '^" + binary + "'"
      rval, lines = self.system (cmd, 3, quiet=True)
      self.log (2, "tryKill: cmd="+cmd+ " rval=" + str(rval) + " lines=" + str(lines))
      if not rval:
        existed = True
        cmd = "pkill -SIG" + signal + " -f '^" + binary + "'"
        rval, lines = self.system (cmd, 1)
    return existed 

    
  def conclude (self):

    self.quit_event.set()

    if self.tryKill ("INT"):
      time.sleep(2)
      if self.tryKill ("TERM"):
        time.sleep(2)
        self.tryKill ("KILL")

    if self.control_thread:
      self.control_thread.join()

    if self.log_sock: 
      self.log_sock.close ()

  def system (self, command, dl=2, quiet=False):
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
    try:
      (output, junk) = proc.communicate()
    except IOError, e:
      if e.errno == errno.EINTR:
        self.quit_event.set()
        return (-1, ("SIGINT"))

    return_code = proc.returncode

    if return_code and not quiet:
      self.log (0, "spip.system: " + command + " failed")

    # Once you have a valid response, split the return output    
    if output:
      lines = output.rstrip('\n').split('\n')
      if dl <= self.dl or return_code:
        for line in lines:
          self.log (0, "system: " + line)

    return return_code, lines

  def system_raw (self, command, dl=2):
    return_code = 0

    self.log (dl, "system: " + command)

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
        self.quit_event.set()
        return (-1, ("SIGINT"))

    return_code = proc.returncode

    if return_code:
      self.log (0, "spip.system: " + command + " failed")

    return return_code, output 


  def system_piped (self, command, pipe, dl=2, env_vars=os.environ.copy()):

    return_code = 0

    self.log(dl, "system_piped: " + command)

    # setup the module object
    proc = subprocess.Popen(command,
                            env=env_vars,
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
