#!/usr/bin/env python

#
# SPIP python module for cornerturn
#

import os, re, socket, datetime, threading, time, sys, atexit, errno
import subprocess

# check if all sub-bands for a beam are contained upon the 1 host
# for a given stream_id

def isLocal (beam_id, host, cfg):

  is_local = True

  for i in int(cfg["NUM_STREAM"]):
    (h, b, s) = cfg["STREAM_" + str(i)].split(":")
    if beam_id == b and h != host:
      is_local = False

  for i in int(cfg["NUM_RECV"]):
    (h, b) = cfg["RECV_" + str(i)].split(":")
    if beam_id == b and h != host:
      is_local = False

  return is_local

def isSendLocal (stream_id, cfg):

  is_local = True

  # get the beam this stream belongs to
  (host, beam_id, subband_id) = cfg["STREAM_" + stream_id].split(":")

  # if this sender is not involved in the cornerturn, it cannot be
  # a local cornerturn
  if beam_id == "-" or subband_id == "-":
    return False

  return isLocal (beam_id, host)

def isRecvLocal (recv_id, cfg):
  is_local = True

  # get the beam this recv belongs to
  (host, beam_id) = cfg["RECV_" + recv_id].split(":")

  return isLocal (beam_id, host)

