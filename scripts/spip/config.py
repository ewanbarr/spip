#!/usr/bin/env python

##############################################################################
#  
#     Copyright (C) 2015 by Andrew Jameson
#     Licensed under the Academic Free License version 2.1
# 
###############################################################################

import os, re, socket, datetime, threading, time, sys, atexit, errno
import subprocess

SPIP_ROOT = os.environ.get('SPIP_ROOT');

def getSPIP_ROOT():
  return SPIP_ROOT

def getConfig():
  config_file = getSpipCFGFile()
  config = readCFGFileIntoDict(config_file)
  return config

def getSiteConfig():
  config_file = getSiteCFGFile()
  config = readCFGFileIntoDict(config_file)
  return config

def getSpipCFGFile():
  return SPIP_ROOT + "/share/spip.cfg"

def getSiteCFGFile():
  return SPIP_ROOT + "/share/site.cfg"

def readCFGFileIntoDict(filename):
  config = {}
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
          config[parts[0]] = parts[1].strip()
    fptr.closed
    return config

def writeDictToCFGFile (cfg, filename):
  try:
    fptr = open(filename, 'w')
  except IOError:
    print "ERROR: cannot open " + filename + " for writing"
  else:
    for key in sorted(cfg.keys()):
      fptr.write(key.ljust(20) + cfg[key] + "\n")
    fptr.close()

def writeDictToString (cfg):
  string = ""
  for key in sorted(cfg.keys()):
    string += (key.ljust(16) + str(cfg[key]) + "\n")
  return string

def readDictFromString (string):
  config = {}
  for line in string.split("\n"):
    line = line.strip()
    line = re.sub("#.*", "", line);
    if line:
      line = re.sub("\s+", " ", line)
      parts = line.split(' ', 1)
      if (len(parts) == 2):
        config[parts[0]] = parts[1].strip()
  return config

def getStreamConfigFixed (site, cfg, id):

  config = {}

  config["HDR_VERSION"] = site["HDR_VERSION"]
  config["HDR_SIZE"]    = site["HDR_SIZE"]
  config["TELESCOPE"]   = site["TELESCOPE"]

  config["RECEIVER"]   = cfg["RECEIVER"]
  config["INSTRUMENT"] = cfg["INSTRUMENT"]

  config["NBIT"]  = cfg["NBIT"]
  config["NPOL"]  = cfg["NPOL"]
  config["NDIM"]  = cfg["NDIM"]
  config["TSAMP"] = cfg["TSAMP"]

  (freq, bw, nchan) = cfg["SUBBAND_CONFIG_" + str(id)].split(":")
  config["FREQ"] = freq
  config["BW"] = bw
  config["NCHAN"] = nchan

  (start_chan, end_chan) = cfg["SUBBAND_CHANS_" + str(id)].split(":")
  config["START_CHANNEL"] = start_chan
  config["END_CHANNEL"]   = end_chan

  # compute bytes_per_second for stream(id)
  nchan_int = int(nchan)
  nbit = int(config["NBIT"])
  npol = int(config["NPOL"])
  ndim = int(config["NDIM"])
  tsamp = float(config["TSAMP"])

  bytes_per_second = (nchan_int * nbit * npol * ndim * 1e6) / (8 * tsamp)
  config["BYTES_PER_SECOND"] = str(bytes_per_second)
  config["RESOLUTION"] = cfg["RESOLUTION"] 

  return config

def getStreamConfig (stream_id, cfg):
  stream_config = cfg["STREAM_" + str(stream_id)]
  (host, beam_id, subband_id) = stream_config.split(":")
  return (host, beam_id, subband_id)

def parseHeader(lines):
  header = {}
  for line in lines:
    parts = line.split()
    if len(parts) > 1:
      header[parts[0]] = parts[1]
  return header

