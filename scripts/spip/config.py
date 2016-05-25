#!/usr/bin/env python

##############################################################################
#  
#     Copyright (C) 2015 by Andrew Jameson
#     Licensed under the Academic Free License version 2.1
# 
###############################################################################

import os, re, socket, datetime, threading, time, sys, atexit, errno
import subprocess

class Config(object):

  def __init__ (self):
    spip_root = os.environ.get('SPIP_ROOT');
  
    config_file = spip_root + "/share/spip.cfg"
    self.config = self.readCFGFileIntoDict (config_file)

    site_file = spip_root + "/share/site.cfg"
    self.site = self.readCFGFileIntoDict (site_file)

  def getSPIP_ROOT(self):
    return self.spip_root

  def getConfig(self):
    return self.config

  def getSiteConfig(self):
    return self.site

  @staticmethod
  def readCFGFileIntoDict(filename):
    cfg = {}
    try:
      fptr = open(filename, 'r')
    except IOError:
      print "ERROR: cannot open " + filename
    else:
      for line in fptr:
        # remove all comments 
        line = line.strip()
        line = re.sub("#.*", "", line);
        if line: 
          line = re.sub("\s+", " ", line)
          parts = line.split(' ', 1)
          if (len(parts) == 2):
            cfg[parts[0]] = parts[1].strip()
      fptr.closed
      return cfg

  @staticmethod
  def writeDictToCFGFile (cfg, filename):
    try:
      fptr = open(filename, 'w')
    except IOError:
      print "ERROR: cannot open " + filename + " for writing"
    else:
      for key in sorted(cfg.keys()):
        fptr.write(key.ljust(20) + cfg[key] + "\n")
      fptr.close()

  @staticmethod
  def writeDictToString (cfg):
    string = ""
    for key in sorted(cfg.keys()):
      string += (key.ljust(16) + str(cfg[key]) + "\n")
    return string

  @staticmethod
  def readDictFromString (string):
    cfg = {}
    for line in string.split("\n"):
      line = line.strip()
      line = re.sub("#.*", "", line);
      if line:
        line = re.sub("\s+", " ", line)
        parts = line.split(' ', 1)
        if (len(parts) == 2):
          cfg[parts[0]] = parts[1].strip()
    return cfg

  def getStreamConfigFixed (self, id):

    cfg = {}

    cfg["HDR_VERSION"] = self.site["HDR_VERSION"]
    cfg["HDR_SIZE"]    = self.site["HDR_SIZE"]
    cfg["TELESCOPE"]   = self.site["TELESCOPE"]

    cfg["RECEIVER"]   = self.config["RECEIVER"]
    cfg["INSTRUMENT"] = self.config["INSTRUMENT"]

    cfg["NBIT"]  = self.config["NBIT"]
    cfg["NPOL"]  = self.config["NPOL"]
    cfg["NDIM"]  = self.config["NDIM"]
    cfg["TSAMP"] = self.config["TSAMP"]

    # determine subband for this stream
    (host, beam, subband) = self.config["STREAM_" + str(id)].split(":")

    (freq, bw, nchan) = self.config["SUBBAND_CONFIG_" + str(subband)].split(":")
    cfg["FREQ"] = freq
    cfg["BW"] = bw
    cfg["NCHAN"] = nchan

    (start_chan, end_chan) = self.config["SUBBAND_CHANS_" + str(subband)].split(":")
    cfg["START_CHANNEL"] = start_chan
    cfg["END_CHANNEL"]   = end_chan

    # compute bytes_per_second for stream(id)
    nchan_int = int(nchan)
    nbit = int(self.config["NBIT"])
    npol = int(self.config["NPOL"])
    ndim = int(self.config["NDIM"])
    tsamp = float(self.config["TSAMP"])

    bytes_per_second = (nchan_int * nbit * npol * ndim * 1e6) / (8 * tsamp)
    cfg["BYTES_PER_SECOND"] = str(bytes_per_second)
    cfg["RESOLUTION"] = self.config["RESOLUTION"] 

    return cfg

  @staticmethod
  def getStreamConfig (stream_id, cfg):
    stream_config = cfg["STREAM_" + str(stream_id)]
    (host, beam_id, subband_id) = stream_config.split(":")
    return (host, beam_id, subband_id)

  @staticmethod
  def parseHeader(lines):
    header = {}
    for line in lines:
      parts = line.split()
      if len(parts) > 1:
        header[parts[0]] = parts[1]
    return header

