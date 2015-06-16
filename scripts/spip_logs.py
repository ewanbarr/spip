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

def processLine (line, header):

  print line


#################################################################
# main

def main (argv):

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
  # spip.daemonize(pid_file, log_file)

  try:

    spip.logMsg(1, DL, "STARTING SCRIPT")

    quit_event = threading.Event()

    def signal_handler(signal, frame):
      quit_event.set()

    signal.signal(signal.SIGINT, signal_handler)

    # start a control thread to handle quit requests
    control_thread = spip.controlThread(quit_file, pid_file, quit_event, DL)
    control_thread.start()

    log_host = cfg["SERVER_HOST"]
    log_port = cfg["SERVER_LOG_PORT"]

    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    spip.logMsg(2, DL, "commandThread: binding to " + log_host + ":" + log_port)
    sock.bind((log_host, int(log_port)))

    sock.listen(10)

    can_read = [sock]
    can_write = []
    can_error = []
    headers = {}
    line_buffers = {}
    timeout = 1

    while (not quit_event.isSet()):

      spip.logMsg(3, DL, "main: calling select len(can_read)="+str(len(can_read)))
      timeout = 1
      did_read, did_write, did_error = select.select(can_read, can_write, can_error, timeout)
      spip.logMsg(3, DL, "main: read="+str(len(did_read))+" write="+ \
                  str(len(did_write))+" error="+str(len(did_error)))

      if (len(did_read) > 0):
        for handle in did_read:
          if (handle == sock):
            (new_conn, addr) = sock.accept()
            spip.logMsg(1, DL, "main: accept connection from "+repr(addr))
            can_read.append(new_conn)
      
            # read header information about this logging stream
            header = new_conn.recv (4096)
            # return a single character to confirm header has been receive
            new_conn.send('\n')
            header = header.strip()
           
            xml = xmltodict.parse(header)

            headers[new_conn] = xml
            line_buffers[new_conn] = ""

          else:

            message = handle.recv(4096)

            # if the socket has been closed
            if len(message) == 0:
              spip.logMsg(1, DL, "commandThread: closing connection")
              if len(line_buffers[handle]) > 0:
                line = line_buffers[handle]
                processLine(line, headers[handle])
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
                spip.logMsg(3, DL, "commandThread: line='" + line + "'")
                processLine (line, headers[handle])

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

if __name__ == "__main__":
  if len(sys.argv) != 2:
    print "ERROR: 1 command line argument expected"
    sys.exit(1)

  main (sys.argv)
  sys.exit(0)


