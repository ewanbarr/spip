##############################################################################
#  
#     Copyright (C) 2015 by Andrew Jameson
#     Licensed under the Academic Free License version 2.1
# 
###############################################################################

import threading, socket, errno
from select import select

class SocketedThread (threading.Thread):

  def __init__ (self, script, host, port):
    threading.Thread.__init__(self)
    self.script = script
    self.host = host
    self.port = port

    self.can_read = []
    self.can_write = []
    self.can_error = []

  def run (self):

    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

    self.script.log (1, "SocketedThread listening on " + self.host + ":" + str(self.port))
    sock.bind((self.host, int(self.port)))
    sock.listen(1)

    self.can_read = [sock]
    self.can_write = []
    self.can_error = []

    while not self.script.quit_event.isSet():

      timeout = 1

      did_read = []
      did_write = []
      did_error = []

      try:
        # wait for some activity on the control socket
        self.script.log (3, "SocketedThread: select")
        did_read, did_write, did_error = select(self.can_read, self.can_write, self.can_error, timeout)
        self.script.log (3, "SocketedThread: read="+str(len(did_read))+" write="+
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
            self.script.log (2, "SocketedThread: accept connection from "+repr(addr))

            # add the accepted connection to can_read
            self.can_read.append(new_conn)

          # an accepted connection must have generated some data
          else:

            try:
              self.process_message_on_handle (handle)

            except socket.error as e:
              if e.errno == errno.ECONNRESET:
                self.script.log (2, "SocketedThread closing connection")
                handle.close()
                for i, x in enumerate(self.can_read):
                  if (x == handle):
                    del self.can_read[i]
              else:
                raise

  def parse_message(self, data):
    return "ok"

