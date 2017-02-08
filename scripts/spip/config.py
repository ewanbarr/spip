#!/usr/bin/env python

##############################################################################
#  
#     Copyright (C) 2015 by Andrew Jameson
#     Licensed under the Academic Free License version 2.1
# 
###############################################################################

import os, re, socket, datetime, threading, time, sys, atexit, errno
import subprocess

from tempfile import mkstemp
from shutil import move

class Config(object):

  def __init__ (self):
    spip_root = os.environ.get('SPIP_ROOT');
  
    self.config_file = spip_root + "/share/spip.cfg"
    self.config = self.readCFGFileIntoDict (self.config_file)

    self.site_file = spip_root + "/share/site.cfg"
    self.site = self.readCFGFileIntoDict (self.site_file)

  def getSPIP_ROOT(self):
    return self.spip_root

  def getConfig(self):
    return self.config

  def getSiteConfig(self):
    return self.site

  def updateKeyValueConfig (self, key, value):
    self.updateKeyValueCFGFile(key, value, self.config_file)

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
        fptr.write(key.ljust(19) + " " + cfg[key] + "\n")
      fptr.close()

  @staticmethod
  def writeDictToColonSVFile (cfg, filename):
    try:
      fptr = open(filename, 'w')
    except IOError:
      print "ERROR: cannot open " + filename + " for writing"
    else:
      for key in sorted(cfg.keys()):
        fptr.write(key + ";" + cfg[key] + "\n")
      fptr.close()

  @staticmethod
  def updateKeyValueCFGFile(key, value, filename):
    try:
      iptr = open(filename, 'r')
    except IOError:
      print "ERROR: cannot open " + filename + " for reading"
    else:
      try:
        optr, abs_path = mkstemp()
      except IOError:
        print "ERROR: cannot open temporary file for writing"
      else:
        for line in iptr:
          if line.startswith(key + " "):
            optr.write(key.ljust(19) + " " + value + "\n")
          else:
            optr.write(line)
        os.close(iptr)
        os.close(optr)
        os.remove (filename)
        move(abs_path, filename)

  @staticmethod
  def writeDictToString (cfg):
    string = ""
    for key in sorted(cfg.keys()):
      string += (key.ljust(19) + " " + str(cfg[key]) + "\n")
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

  @staticmethod
  def mergeHeaderFreq (h1, h2):

    header = h1

    header["BYTES_PER_SECOND"] = str (int(h1["BYTES_PER_SECOND"]) + int(h2["BYTES_PER_SECOND"]))
    header["NCHAN"] = str (int(h1["NCHAN"]) + int(h2["NCHAN"]))
    header["START_CHANNEL"] = str (min(int(h1["START_CHANNEL"]), int(h2["START_CHANNEL"])))
    header["END_CHANNEL"] = str (max(int(h1["END_CHANNEL"]), int(h2["END_CHANNEL"])))

    bw1 = float(h1["BW"])
    bw2 = float(h2["BW"])
    freq1 = float(h1["FREQ"])
    freq2 = float(h2["FREQ"])

    header["BW"] = str (bw1 + bw2)
    header["FREQ"] = str ((freq1 - bw1/2) + ((bw1+bw2)/2))

    return header

  def getStreamConfigFixed (self, id):

    cfg = {}

    cfg["HDR_VERSION"] = self.site["HDR_VERSION"]
    cfg["HDR_SIZE"]    = self.site["HDR_SIZE"]
    cfg["TELESCOPE"]   = self.site["TELESCOPE"]
    cfg["DSB"]         = self.site["DSB"]

    cfg["RECEIVER"]   = self.config["RECEIVER"]
    cfg["INSTRUMENT"] = self.config["INSTRUMENT"]

    cfg["NBIT"]  = self.config["NBIT"]
    cfg["NPOL"]  = self.config["NPOL"]
    cfg["NDIM"]  = self.config["NDIM"]
    cfg["TSAMP"] = self.config["TSAMP"]

    # determine subband for this stream
    (host, beam, subband) = self.config["STREAM_" + str(id)].split(":")
    cfg["STREAM_HOST"] = host
    cfg["STREAM_BEAM_ID"] = beam 
    cfg["STREAM_SUBBAND_ID"] = subband 

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

