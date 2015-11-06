##############################################################################
#  
#     Copyright (C) 2015 by Andrew Jameson
#     Licensed under the Academic Free License version 2.1
# 
###############################################################################

class Basis:

  def __init__ (self, type):
    self.basis = type

  def getBasis (self):
    return self.basis

class ServerBased(Basis):

  def __init__ (self, cfg):
    Basis.__init__(self, "server")
    self.id = "-1"
    self.req_host = cfg["SERVER_HOST"]
    self.log_dir  = cfg["SERVER_LOG_DIR"]
    self.control_dir = cfg["SERVER_CONTROL_DIR"]

class BeamBased(Basis):
  
  def __init__ (self, id, cfg):
    Basis.__init__(self, "beam")
    self.id = id
    (self.req_host, self.beam_id) = self.getConfig(id, cfg)
    self.log_dir     = cfg["SERVER_LOG_DIR"]
    self.control_dir = cfg["SERVER_CONTROL_DIR"]

  def getConfig (self, id, cfg):
    stream_config = cfg["STREAM_" + str(id)]
    (host, beam_id, subband_id) = stream_config.split(":")
    return (host, beam_id)

class StreamBased(Basis):
  
  def __init__ (self, id, cfg):
    Basis.__init__(self, "stream")
    self.id = id
    (self.req_host, self.beam_id, self.subband_id) = self.getConfig(id, cfg)
    self.log_dir     = cfg["CLIENT_LOG_DIR"]
    self.control_dir = cfg["CLIENT_CONTROL_DIR"]

  def getConfig (self, id, cfg):
    stream_config = cfg["STREAM_" + str(id)]
    (host, beam_id, subband_id) = stream_config.split(":")
    return (host, beam_id, subband_id)

class RecvBased(Basis):

  def __init__ (self, id, cfg):
    Basis.__init__(self, "recv")
    self.id = id
    (self.req_host, self.beam_id) = self.getConfig(id, cfg)
    self.log_dir     = self.cfg["CLIENT_LOG_DIR"]
    self.control_dir = self.cfg["CLIENT_CONTROL_DIR"]

  def getConfig (self, id, cfg):
    stream_config = cfg["STREAM_" + str(id)]
    (host, beam_id) = stream_config.split(":")
    return (host, beam_id)

class HostBased(Basis):

  def __init__ (self, hostname, cfg):
    Basis.__init__(self, "host")
    self.id = hostname
    self.req_host = hostname
    if self.req_host == cfg["SERVER_HOST"]:
      self.log_dir     = cfg["SERVER_LOG_DIR"]
      self.control_dir = cfg["SERVER_CONTROL_DIR"]
    else:
      self.log_dir     = self.cfg["CLIENT_LOG_DIR"]
      self.control_dir = self.cfg["CLIENT_CONTROL_DIR"]

