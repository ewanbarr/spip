##############################################################################
#  
#     Copyright (C) 2015 by Andrew Jameson
#     Licensed under the Academic Free License version 2.1
# 
###############################################################################

from daemon import Daemon

class ServerDaemon(Daemon):

  def __init__ (self, name):
    Daemon.__init__(self, name, "-1")

    # check that independent beams is off
    if self.cfg["INDEPENDENT_BEAMS"] == "1":
      raise Exception ("ServerDaemons incompatible with INDEPENDENT_BEAMS")  
    self.req_host = self.cfg["SERVER_HOST"]
    self.log_dir  = self.cfg["SERVER_LOG_DIR"]
    self.control_dir = self.cfg["SERVER_CONTROL_DIR"]

  def getType (self):
    return "serv"

