##############################################################################
#  
#     Copyright (C) 2015 by Andrew Jameson
#     Licensed under the Academic Free License version 2.1
# 
###############################################################################

import threading, socket, errno
from xmltodict import parse
from select import select

class ReportingThread (threading.Thread):

  def __init__ (self, script, host, port):
    threading.Thread.__init__(self)
    self.script = script
    self.host = host
    self.port = port

  def run (self):

    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

    self.script.log (1, "ReportingThread listening on " + self.host + ":" + str(self.port))
    sock.bind((self.host, int(self.port)))
    sock.listen(1)

    can_read = [sock]
    can_write = []
    can_error = []

    while not self.script.quit_event.isSet():

      timeout = 1

      did_read = []
      did_write = []
      did_error = []


      try:
        # wait for some activity on the control socket
        self.script.log (3, "ReportingThread: select")
        did_read, did_write, did_error = select(can_read, can_write, can_error, timeout)
        self.script.log (3, "ReportingThread: read="+str(len(did_read))+" write="+
                   str(len(did_write))+" error="+str(len(did_error)))
      except select.error as e:
        if e[0] == errno.EINTR:
          self.script.log(0, "SIGINT received during select, exiting")
          self.script.quit_event.set()
        else:
          raise

      if (len(did_read) > 0):
        for handle in did_read:
          if (handle == sock):
            (new_conn, addr) = sock.accept()
            self.script.log (2, "ReportingThread: accept connection from "+repr(addr))

            # add the accepted connection to can_read
            can_read.append(new_conn)

          # an accepted connection must have generated some data
          else:
            try:
              raw = handle.recv(4096)
              message = raw.strip()

              self.script.log (1, "ReportingThread message='" + message+"'")
              xml = parse(message)
              self.script.log(3, "<- " + str(xml))

              reply = self.parse_message (xml)

              handle.send (reply)

            except socket.error as e:
              if e.errno == errno.ECONNRESET:
                self.script.log (1, "ReportingThread closing connection")
                handle.close()
                for i, x in enumerate(can_read):
                  if (x == handle):
                    del can_read[i]
              else:
                raise

  def parse_message(self, xml):
    return "ok"

