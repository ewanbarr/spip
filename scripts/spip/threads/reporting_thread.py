##############################################################################
#  
#     Copyright (C) 2015 by Andrew Jameson
#     Licensed under the Academic Free License version 2.1
# 
###############################################################################

from socketed_thread import SocketedThread

from xmltodict import parse
from xml.parsers.expat import ExpatError
import socket

class ReportingThread (SocketedThread):

  def __init__ (self, script, host, port):
    SocketedThread.__init__(self, script, host, port)

  def run (self):
    SocketedThread.run(self)

  def process_message_on_handle (self, handle):

    raw = handle.recv(4096)
    message = raw.strip()
    self.script.log (2, "ReportingThread: message="+str(message))

    if len(message) == 0:
      handle.close()
      for i, x in enumerate(self.can_read):
        if (x == handle):
          del self.can_read[i]

    else:
      try:
       xml = parse(message)
      except ExpatError as e:
        handle.send ("<xml>Malformed XML message</xml>\r\n")
        handle.close()
        for i, x in enumerate(self.can_read):
          if (x == handle):
            del self.can_read[i]

      self.script.log(3, "<- " + str(xml))

      retain, reply = self.parse_message (xml)

      if retain:
        bytes_sent = handle.send (reply)
        self.script.log (2, "ReportingThread: sent " + str(bytes_sent) + " bytes")
      else:
        bytes_sent = handle.sendall (reply)
        self.script.log (2, "ReportingThread: sent " + str(bytes_sent) + " bytes")

        self.script.log (3, "ReportingThread: handle.shutdown()")
        handle.shutdown(socket.SHUT_RDWR)
        self.script.log (3, "ReportingThread: handle.close()")
        handle.close()
        for i, x in enumerate(self.can_read):
          if (x == handle):
                del self.can_read[i]

