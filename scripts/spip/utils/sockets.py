##############################################################################
#  
#     Copyright (C) 2015 by Andrew Jameson
#     Licensed under the Academic Free License version 2.1
# 
###############################################################################

import socket, errno
from time import sleep
from core import logMsg

def getHostNameShort ():
  return socket.gethostname().split(".", 1)[0]

def getHostMachineName():
  fqdn = socket.gethostname()
  parts = fqdn.split('.',1)
  if (len(parts) >= 1):
    host = parts[0]
  if (len(parts) == 2):
    domain = parts[1]
  return host

###############################################################################
# open a standard socket
def openSocket(dl, host, port, attempts=10):

  sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
  connected = False

  while (not connected and attempts > 0):

    logMsg(3, dl, "openSocket: attempt " + str(11-attempts))

    try:
      sock.connect((host, port))

    except socket.error, e:
      if e.errno == errno.ECONNREFUSED:
        logMsg(2, dl, "openSocket: connection to " + host + ":" + str(port) + " refused")
        attempts -= 1
        if  attempts > 0:
          sleep(1)
      else:
        raise
    else:
      logMsg(3, dl, "openSocket: connected")
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
