#!/usr/bin/env python

##############################################################################
#  
#     Copyright (C) 2015 by Andrew Jameson
#     Licensed under the Academic Free License version 2.1
# 
###############################################################################

from spip.config import Config

class MeerKATConfig(Config):

  def __init__ (self):
    Config.__init__(self)

  def getStreamConfigFixed (self, id):

    cfg = Config.getStreamConfigFixed (self, id)

    (cfg["DATA_HOST_0"], cfg["DATA_HOST_1"]) = self.config["DATA_HOST_0"].split(",")
    (cfg["DATA_MCAST_0"], cfg["DATA_MCAST_1"]) = self.config["DATA_MCAST_0"].split(",")
    (cfg["DATA_PORT_0"], cfg["DATA_PORT_1"]) = self.config["DATA_PORT_0"].split(",")

    (cfg["META_HOST_0"], cfg["META_HOST_1"]) = self.config["META_HOST_0"].split(",")
    (cfg["META_MCAST_0"], cfg["META_MCAST_1"]) = self.config["META_MCAST_0"].split(",")
    (cfg["META_PORT_0"], cfg["META_PORT_1"]) = self.config["META_PORT_0"].split(",")

    cfg["ADC_SAMPLE_RATE"] = self.config["ADC_SAMPLE_RATE"]

    (freq, bw, nchan) = self.config["SUBBAND_CONFIG_" + cfg["STREAM_SUBBAND_ID"]].split(":")
    chan_bw = float(bw) / float(nchan)
    cfg["FREQ"] = str(float(freq) - chan_bw / 2)

    return cfg

