#!/usr/bin/env python

##############################################################################
#  
#     Copyright (C) 2015 by Andrew Jameson
#     Licensed under the Academic Free License version 2.1
# 
###############################################################################

from daemon import Daemon

class RecvDaemon (Daemon):

  def __init__ (self, name, id):
    Daemon.__init__(self, name, id)
    (self.req_host, self.beam_id) = self.getConfig(id)
    self.subband_id = 1

  def getConfig (self, id):
    stream_config = self.cfg["RECV_" + str(id)]
    (host, beam_id) = stream_config.split(":")
    return (host, beam_id)

  def getType (self):
    return "recv"

