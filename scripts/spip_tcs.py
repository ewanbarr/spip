#!/usr/bin/env python

###############################################################################
#  
#     Copyright (C) 2015 by Andrew Jameson
#     Licensed under the Academic Free License version 2.1
# 
###############################################################################

import os, threading, sys, socket, select, signal, traceback, xmltodict
import errno

from spip import config
from spip.daemons.bases import ServerBased,BeamBased
from spip.daemons.daemon import Daemon
from spip.log_socket import LogSocket
from spip.utils import sockets,times
from spip.threads.reporting_thread import ReportingThread

DAEMONIZE = False
DL = 1

###############################################################
# thread for reporting state of 
class TCSReportingThread (ReportingThread):

  def __init__ (self, script, id):
    host = sockets.getHostNameShort()
    port = int(script.cfg["TCS_REPORT_PORT"]) + int(id)
    ReportingThread.__init__(self, script, host, port)

  def parse_message (self, xml):
    self.script.log (0, "TCSReportingThread::parse_message: " + str(xml))

    xml  = "<tcs_state>"
    for beam in self.beams:
      xml += "<beam id='" + str(beam) + "'>"

      xml += "<source>"
      xml += "<name epoch='J2000'></name>"
      xml += "<ra units='hh:mm:ss'></ra>"
      xml += "<dec units='hh:mm:ss'></dec>"
      xml += "</source>"        

      xml += "<observation>"
      xml += "<length units='seconds'></length>"
      xml += "<snr></snr>"
      xml += "<expected_length units='seconds'></expected_length>"
      xml += "</observation>"

      xml += "<signal>"
      xml += "<nbit></nbit>"
      xml += "<npol></npol>"
      xml += "<ndim></ndim>"
      xml += "<nchan></nchan>"
      xml += "<tsamp></tsamp>"
      xml += "<cfreq></cfreq>"
      xml += "<bw></bw>"
      xml += "</signal>"

      xml += "<instrument>" 
      xml += "<state></state>"
      xml += "<hardware>"
      xml += "<server hostname=''>"
      xml += "</server>"
      xml += "</hardware>"
      xml += "</instrument>" 

      xml += "</beam>"

    xml += "</tcs_state>"

    return "ok"

###############################################################
# TCS daemon
class TCSDaemon(Daemon):

  def __init__ (self, name, id):
    Daemon.__init__(self, name, str(id))

  def main (self, id):

    hostname = sockets.getHostNameShort()
    port = int(self.cfg["TCS_INTERFACE_PORT"])
    if int(id) == -1:
      self.log(2, "main: TCS listening on " + hostname + ":" + str(port))
    elif int(id) >= 0 and int(id) < int(self.cfg["NUM_BEAM"]):
      port += int(id)
      self.log(2, "main: TCS listening on " + hostname + ":" + str(port))
    else:
      self.log(0, "main: bad id")
      sys.exit(1)

    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

    sock.bind((hostname, port))
    sock.listen(1)

    can_read = [sock]
    can_write = []
    can_error = []

    while not self.quit_event.isSet():

      timeout = 1

      did_read = []
      did_write = []
      did_error = []

      try:
        # wait for some activity on the control socket
        self.log(3, "main: select")
        did_read, did_write, did_error = select.select(can_read, can_write, can_error, timeout)
        self.log(3, "main: read="+str(len(did_read))+" write="+
                  str(len(did_write))+" error="+str(len(did_error)))

      except select.error as e:
        if e[0] == errno.EINTR:
          self.log(0, "SIGINT received during select, exiting")
          self.quit_event.set()

      if (len(did_read) > 0):
        for handle in did_read:
          if (handle == sock):
            (new_conn, addr) = sock.accept()
            self.log(1, "main: accept connection from "+repr(addr))

            # add the accepted connection to can_read
            can_read.append(new_conn)

          # an accepted connection must have generated some data
          else:

            try:

              raw = handle.recv(4096)

              message = raw.strip()
              self.log(3, "commandThread: message='" + message+"'")
              xml = xmltodict.parse(message)
              self.log(3, "<- " + str(xml))

              # Parse XML for correctness
              (valid, command, obs) = self.parse_obs_cmd (xml, id)

              if valid:
                self.issue_cmd (xml, command, obs)
              else:
                self.log(-1, "failed to parse xml: " + obs)

              response = "OK"
              self.log(3, "-> " + response)
              xml_response = "<?xml version='1.0' encoding='ISO-8859-1'?>" + \
                             "<gen_response>" + response + "</gen_response>"
              handle.send (xml_response + "\r\n")
    
            except socket.error as e:
              if e.errno == errno.ECONNRESET:
                self.log(1, "commandThread: closing connection")
                handle.close()
                for i, x in enumerate(can_read):
                  if (x == handle):
                    del can_read[i]

              else:
                raise

  ###############################################################################
  # parse an XML command for correctness
  def parse_obs_cmd (self, xml, id):

    # observation specific parameters
    obs = {}
    header = {}
    command = ""

    try:
      command = xml['obs_cmd']['command']

      if command == "start":
        
        # acquire the obs params first
        obs["COMMAND"]    = "START"
        obs["SOURCE"] = xml['obs_cmd']['source_parameters']['name']['#text']
        obs["RA"]     = xml['obs_cmd']['source_parameters']['ra']['#text']
        obs["DEC"]    = xml['obs_cmd']['source_parameters']['dec']['#text']

        obs["OBSERVER"]  = xml['obs_cmd']['observation_parameters']['observer']
        obs["PID"]       = xml['obs_cmd']['observation_parameters']['project_id']
        obs["MODE"]      = xml['obs_cmd']['observation_parameters']['mode']
        obs["PROC_FILE"] = xml['obs_cmd']['observation_parameters']['processing_file']

        obs["UTC_START"]  = xml['obs_cmd']['observation_parameters']['utc_start']
        obs["OBS_OFFSET"] = "0"
        obs["TOBS"]       = xml['obs_cmd']['observation_parameters']['tobs']

      else:
        obs["COMMAND"]  = "STOP"
        obs["UTC_STOP"] = xml['obs_cmd']['observation_parameters']['utc_stop']

      # merge header and obs params
      # header.update(obs)

    except KeyError as e:
      return (False, "none", "Could not find key " + str(e))

    return (True, command, obs)

  ###############################################################################
  # issue_cmd
  def issue_cmd (self, xml, command, obs):

    self.beams = []

    for ibeam in range(int(xml['obs_cmd']['beam_configuration']['nbeam'])):
      if xml['obs_cmd']['beam_configuration']['beam_state_' + str(ibeam)] == "on":
        self.beams.append(ibeam)

    # TODO work out how to determine UTC_START
    self.log(1, "issue_cmd: UTC_START=" + str(obs["UTC_START"]))
    if obs["UTC_START"] == None:
      obs["UTC_START"] = times.getUTCTime()
      self.log(1, "issue_cmd: setting UTC_START=" + obs["UTC_START"])

    obs["PERFORM_FOLD"] = "1"
    obs["PERFORM_SEARCH"] = "0"
    obs["PERFORM_TRANS"] = "0"

    # convert
    obs_header = config.writeDictToString (obs)

    self.log(1, "issue_cmd: beams=" + str(self.beams) + " num_stream=" + self.cfg["NUM_STREAM"])

    # work out which streams correspond to these beams
    for istream in range(int(self.cfg["NUM_STREAM"])):
      (host, beam, subband) = self.cfg["STREAM_"+str(istream)].split(":")
      self.log(1, "issue_cmd: host="+host+"beam="+beam+"subband="+subband)

      if int(beam) in self.beams:

        # control port the this recv stream 
        ctrl_port = int(self.cfg["STREAM_CTRL_PORT"]) + istream

        # connect to recv agent and provide observation configuration
        self.log(1, "issue_cmd: openSocket("+host+","+str(ctrl_port)+")")
        sock = sockets.openSocket (DL, host, ctrl_port, 1)
        if sock:
          sock.send(obs_header)
          sock.close()

        # connect to spip_gen and issue start command for UTC
        # assumes gen host is the same as the recv host!
        gen_port = int(self.cfg["STREAM_GEN_PORT"]) + istream
        sock = sockets.openSocket (DL, host, gen_port, 1)
        if sock:
          sock.send(obs_header)
          sock.close()
      else:
        self.log(1, "issue_cmd: beam="+beam+" not valid")


class TCSServerDaemon (TCSDaemon, ServerBased):

  def __init__ (self, name):
    TCSDaemon.__init__(self,name, "-1")
    ServerBased.__init__(self, self.cfg)


class TCSBeamDaemon (TCSDaemon, BeamBased):

  def __init__ (self, name, id):
    TCSDaemon.__init__(self, name, str(id))
    BeamBased.__init__(self, str(id), self.cfg)

###############################################################################
#
if __name__ == "__main__":

  if len(sys.argv) != 2:
    print "ERROR: at most 1 command line argument expected"
    sys.exit(1)

  # this should come from command line argument
  beam_id = sys.argv[1]

  # if the beam_id is < 0, then there is a single TCS for 
  # all beams, otherwise, 1 per beam
    
  if int(beam_id) == -1:
    script = TCSServerDaemon ("tcs")
    beam_id = 0
  else:
    script = TCSBeamDaemon ("tcs", beam_id)

  state = script.configure (DAEMONIZE, DL, "tcs", "tcs")
  if state != 0:
    sys.exit(state)

  script.log(1, "STARTING SCRIPT")

  try:
   
    reporting_thread = TCSReportingThread(script, beam_id)
    reporting_thread.start()

    script.main (beam_id)

    reporting_thread.join()

  except:

    script.quit_event.set()

    script.log(-2, "exception caught: " + str(sys.exc_info()[0]))
    print '-'*60
    traceback.print_exc(file=sys.stdout)
    print '-'*60

  script.log(1, "STOPPING SCRIPT")
  script.conclude()
  sys.exit(0)
