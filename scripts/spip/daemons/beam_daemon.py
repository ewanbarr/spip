##############################################################################
#  
#     Copyright (C) 2015 by Andrew Jameson
#     Licensed under the Academic Free License version 2.1
# 
###############################################################################

from daemon import Daemon

class BeamDaemon (Daemon):

  def __init__ (self, name, id):
    Daemon.__init__(self, name, id)
    (self.req_host, self.beam_id) = self.getConfig(id)
    self.log_dir  = self.cfg["SERVER_LOG_DIR"]
    self.control_dir = self.cfg["SERVER_CONTROL_DIR"]

  def getConfig (self, id):
    stream_config = self.cfg["STREAM_" + str(id)]
    print stream_config
    (host, beam_id) = stream_config.split(":")
    return (host, beam_id)

  def getType (self):
    return "beam"


