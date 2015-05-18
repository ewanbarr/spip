#!/usr/bin/env python

##############################################################################
#  
#     Copyright (C) 2015 by Andrew Jameson
#     Licensed under the Academic Free License version 2.1
# 
###############################################################################

import os, threading, sys, time, socket, select, signal, traceback, xmltodict
import spip

SCRIPT = "spip_logs"
DL     = 2

#################################################################
#
# main
#

# read configuration file
cfg = spip.getConfig()

control_thread = []

log_file  = cfg["SERVER_LOG_DIR"] + "/" + SCRIPT + ".log"
pid_file  = cfg["SERVER_CONTROL_DIR"] + "/" + SCRIPT + ".pid"
quit_file = cfg["SERVER_CONTROL_DIR"] + "/"  + SCRIPT + ".quit"

if os.path.exists(quit_file):
  sys.stderr.write("quit file existed at launch: " + quit_file)
  sys.exit(1)

# become a daemon
spip.daemonize(pid_file, log_file)

try:

  spip.logMsg(1, DL, "STARTING SCRIPT")

  quit_event = threading.Event()

  def signal_handler(signal, frame):
    quit_event.set()

  signal.signal(signal.SIGINT, signal_handler)

  # start a control thread to handle quit requests
  control_thread = spip.controlThread(quit_file, pid_file, quit_event, DL)
  control_thread.start()

  log_host = cfg["LOG_HOST"]
  log_port = cfg["LOG_PORT"]

  sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
  sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
  Dada.logMsg(2, DL, "commandThread: binding to " + log_host + ":" + log_port)
  sock.bind((log_host, int(log_port)))

  sock.listen(2)

  can_read = [sock]
  can_write = []
  can_error = []
  timeout = 1

  while (not quit_event.isSet()):

    spip.logMsg(3, DL, "main: calling select len(can_read)="+str(len(can_read)))
    timeout = 1
    did_read, did_write, did_error = select.select(can_read, can_write, can_error, timeout)
    spip.logMsg(3, DL, "main: read="+str(len(did_read))+" write="+str(len(did_write))+" error="+str(len(did_error)))

    if (len(did_read) > 0):
      for handle in did_read:
        if (handle == sock):
          (new_conn, addr) = sock.accept()
          Dada.logMsg(1, DL, "main: accept connection from "+repr(addr))
          can_read.append(new_conn)

        else:
          message = handle.recv(4096)
          # remove trailing whitespace
          message = message.strip()
          Dada.logMsg(3, DL, "commandThread: message='" + message+"'")
          if (len(message) == 0):
            Dada.logMsg(1, DL, "commandThread: closing connection")
            handle.close()
            for i, x in enumerate(can_read):
              if (x == handle):
                del can_read[i]
          else:
            # message should be xml with the form
            # <log level="INFO|WARN|ERROR" stream_id="0" host="spip-0" source="spip_recv">
            # <time>????-??-??-??:??:??.???</time>
            # <text>The contents of the log message</text>
            # </log>
            xml = xmltodict.parse(message)
            print xml
            

except:
  spip.logMsg(-2, DL, "main: exception caught: " + str(sys.exc_info()[0]))
  print '-'*60
  traceback.print_exc(file=sys.stdout)
  print '-'*60
  quit_event.set()

# join threads
spip.logMsg(2, DL, "main: joining control thread")
if (control_thread):
  control_thread.join()

spip.logMsg(1, DL, "STOPPING SCRIPT")

sys.exit(0)
