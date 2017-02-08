#!/usr/bin/env python

###############################################################################
#  
#     Copyright (C) 2015 by Andrew Jameson
#     Licensed under the Academic Free License version 2.1
# 
###############################################################################

import os, threading, sys, socket, select, signal, traceback, xmltodict
import errno

from spip.config import Config
from spip.daemons.bases import ServerBased,BeamBased
from spip.daemons.daemon import Daemon
from spip.log_socket import LogSocket
from spip.utils import sockets,times
from spip.threads.reporting_thread import ReportingThread

DAEMONIZE = True
DL = 1

###############################################################
# thread for reporting state of 
class TCSReportingThread (ReportingThread):

  def __init__ (self, script, id):
    ReportingThread.__init__(self, script, script.host, script.report_port)
    self.beam_states = script.beam_states
    self.script.log(0, "beam_states=" + str(self.beam_states))
    self.beams = self.beam_states.keys()
    self.script.log(0, "beams=" + str(self.beams))


  def parse_message (self, xml):
    self.script.log (2, "TCSReportingThread::parse_message: " + str(xml))

    xml  = "<tcs_state>"
    for beam in self.beams:

      self.beam_states[beam]["lock"].acquire()

      xml += "<beam name='" + str(beam) + "' state='" + self.beam_states[beam]["state"] + "'>"

      xml += "<source>"
      xml += "<name epoch='J2000'>" + self.beam_states[beam]["source"] + "</name>"
      xml += "<ra units='hh:mm:ss'>" + self.beam_states[beam]["ra"] + "</ra>"
      xml += "<dec units='hh:mm:ss'>" + self.beam_states[beam]["dec"] + "</dec>"
      xml += "</source>"        

      xml += "<observation_parameters>"
      xml += "<observer>" + self.beam_states[beam]["observer"] + "</observer>"
      xml += "<pid>" + self.beam_states[beam]["pid"] + "</pid>"
      xml += "<mode>" + self.beam_states[beam]["mode"] + "</mode>"
      if self.beam_states[beam]["utc_start"] != None:
        utc_start = self.beam_states[beam]["utc_start"]
        self.script.log (2, "TCSReportingThread::parse_message: utc_start=" + str(self.beam_states[beam]["utc_start"]))
        elapsed_time = str(times.diffUTCTime(self.beam_states[beam]["utc_start"]))
      else:
        utc_start = ""
        elapsed_time = ""
  
      xml += "<utc_start>" + utc_start + "</utc_start>"
      xml += "<elapsed_time units='seconds'>" + elapsed_time + "</elapsed_time>"
      xml += "<expected_length units='seconds'>" + self.beam_states[beam]["tobs"] + "</expected_length>"
      xml += "</observation_parameters>"

      xml += "</beam>"
      
      self.beam_states[beam]["lock"].release()

    xml += "</tcs_state>\r\n"

    self.script.log (3, "TCSReportingThread::parse_message: returning " + str(xml))

    return True, xml

###############################################################
# TCS daemon
class TCSDaemon(Daemon):

  def __init__ (self, name, id):
    Daemon.__init__(self, name, str(id))
    self.beam_states = {}
    self.host = sockets.getHostNameShort()


  def main (self, id):

    self.log(2, "main: TCS listening on " + self.host + ":" + str(self.interface_port))
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

    sock.bind((self.host, self.interface_port))
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
        # wait for some activity on the socket
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

              if len(message) > 0:
                self.log(2, "commandThread: message='" + message+"'")
                xml = xmltodict.parse(message)
                self.log(3, "<- " + str(xml))

                # Parse XML for correctness
                (valid, command, error) = self.parse_obs_cmd (xml, id)

                if valid :
                  if command == "start":
                    self.log(2, "commandThread: issue_start_cmd")
                    self.issue_start_cmd (xml)
                  elif command == "stop":
                    self.log(2, "commandThread: issue_stop_cmd")
                    self.issue_stop_cmd (xml)
                  elif command == "configure":
                    self.log(2, "commandThread: no action for configure command")
            
                else:
                  self.log(-1, "failed to parse xml: " + error)

                response = "OK"
                self.log(3, "-> " + response)
                xml_response = "<?xml version='1.0' encoding='ISO-8859-1'?>" + \
                               "<tcs_response>" + response + "</tcs_response>"
                handle.send (xml_response + "\r\n")

              else:
                self.log(1, "commandThread: closing socket on 0 byte message")
                handle.close()
                for i, x in enumerate(can_read):
                  if (x == handle):
                    del can_read[i]
      
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

    command = ""

    try:

      command = xml['obs_cmd']['command']

      # determine which beams this command corresponds to
      for ibeam in range(int(xml['obs_cmd']['beam_configuration']['nbeam'])):
        if xml['obs_cmd']['beam_configuration']['beam_state_' + str(ibeam)]['#text'] == "on":

          b = xml['obs_cmd']['beam_configuration']['beam_state_' + str(ibeam)]['@name']

          # if the beam in the XML command is one of the beams managed by
          # this instance of spip_tcs
          if b in self.beam_states.keys():

            if command == "configure":

              self.log(2, "parse_obs_cmd: received confiuration for beam " + b)
              self.beam_states[b]["lock"].acquire()

              self.beam_states[b]["source"] = xml['obs_cmd']['source_parameters']['name']['#text']
              self.beam_states[b]["ra"]     = xml['obs_cmd']['source_parameters']['ra']['#text']
              self.beam_states[b]["dec"]    = xml['obs_cmd']['source_parameters']['dec']['#text']

              self.beam_states[b]["observer"] = str(xml['obs_cmd']['observation_parameters']['observer'])
              self.beam_states[b]["pid"] = str(xml['obs_cmd']['observation_parameters']['project_id'])
              self.beam_states[b]["mode"] = xml['obs_cmd']['observation_parameters']['mode']
              self.beam_states[b]["calfreq"] = xml['obs_cmd']['observation_parameters']['calfreq']
              self.beam_states[b]["proc_file"] = str(xml['obs_cmd']['observation_parameters']['processing_file'])

              self.beam_states[b]["tobs"] = str(xml['obs_cmd']['observation_parameters']['tobs'])

              # custom fields for this instrument (e.g. adc_sync_time on meerkat)
              self.beam_states[b]["custom_fields"] = str(xml['obs_cmd']['custom_parameters']['fields'])
              for f in self.beam_states[b]["custom_fields"].split(' '):
                self.log(2, "parse_obs_cmd: custom field " + f + "=" + str(xml['obs_cmd']['custom_parameters'][f]))
                self.beam_states[b][f] = str(xml['obs_cmd']['custom_parameters'][f])

              self.beam_states[b]["utc_start"] = None
              self.beam_states[b]["utc_stop"]  = None
              self.beam_states[b]["state"]     = "Configured"

              self.beam_states[b]["lock"].release()

            elif command == "start":

              self.beam_states[b]["lock"].acquire()
              self.beam_states[b]["state"] = "Starting"
              self.beam_states[b]["utc_start"] = xml['obs_cmd']['observation_parameters']['utc_start']
              self.beam_states[b]["lock"].release()

            elif command == "stop":
  
              self.beam_states[b]["lock"].acquire()
              self.beam_states[b]["state"] = "Stopping"
              self.beam_states[b]["utc_stop"] = xml['obs_cmd']['observation_parameters']['utc_stop']
              self.beam_states[b]["lock"].release()

            else:
              self.log(-1, "parse_obs_cmd: unrecognized command " + command)

    except KeyError as e:
      return (False, "none", "Could not find key " + str(e))

    return (True, command, "")

  ###############################################################################
  # issue_start_cmd
  def issue_start_cmd (self, xml):

    # determine which beams this command corresponds to
    for ibeam in range(int(xml['obs_cmd']['beam_configuration']['nbeam'])):
      if xml['obs_cmd']['beam_configuration']['beam_state_' + str(ibeam)]['#text'] == "on":
        b = xml['obs_cmd']['beam_configuration']['beam_state_' + str(ibeam)]['@name']
        if b in self.beam_states.keys():
          obs = {}

          self.beam_states[b]["lock"].acquire()
          obs["COMMAND"] = "START"
          obs["SOURCE"] = self.beam_states[b]["source"]
          obs["RA"] = self.beam_states[b]["ra"]
          obs["DEC"] = self.beam_states[b]["dec"]
          obs["TOBS"] = self.beam_states[b]["tobs"]
          obs["OBSERVER"] = self.beam_states[b]["observer"]
          obs["PID"] = self.beam_states[b]["pid"]
          obs["MODE"] = self.beam_states[b]["mode"]
          obs["CALFREQ"] = self.beam_states[b]["calfreq"]
          obs["OBS_OFFSET"] = "0"

          # if no UTC_START has been specified, set it to +5 seconds
          if self.beam_states[b]["utc_start"] == None:
            self.beam_states[b]["utc_start"] = times.getUTCTime(5)
          obs["UTC_START"] = self.beam_states[b]["utc_start"]

          # inject custom fields into header
          for f in self.beam_states[b]["custom_fields"].split(' '):
            obs[f.upper()] = self.beam_states[b][f]

          self.beam_states[b]["lock"].release()

          obs["PERFORM_FOLD"] = "1"
          obs["PERFORM_SEARCH"] = "0"
          obs["PERFORM_TRANS"] = "0"

          # convert to a single ascii string
          obs_header = Config.writeDictToString (obs)

          self.log(1, "issue_start_cmd: beam=" + b)

          # work out which streams correspond to these beams
          for istream in range(int(self.cfg["NUM_STREAM"])):
            (host, beam_idx, subband) = self.cfg["STREAM_"+str(istream)].split(":")
            beam = self.cfg["BEAM_" + beam_idx]
            self.log(2, "issue_start_cmd: host="+host+" beam="+beam+" subband="+subband)

            # connect to streams for this beam only
            if beam == b:

              # control port the this recv stream 
              ctrl_port = int(self.cfg["STREAM_CTRL_PORT"]) + istream

              # connect to recv agent and provide observation configuration
              self.log(3, "issue_start_cmd: openSocket("+host+","+str(ctrl_port)+")")
              recv_sock = sockets.openSocket (DL, host, ctrl_port, 1)
              if recv_sock:
                self.log(3, "issue_start_cmd: sending obs_header")
                recv_sock.send(obs_header)
                self.log(3, "issue_start_cmd: header sent")
                recv_sock.close()
                self.log(3, "issue_start_cmd: socket closed")

              # connect to spip_gen and issue start command for UTC
              # assumes gen host is the same as the recv host!
              # gen_port = int(self.cfg["STREAM_GEN_PORT"]) + istream
              # sock = sockets.openSocket (DL, host, gen_port, 1)
              # if sock:
              #   sock.send(obs_header)
              #   sock.close()

          # update the dict of observing info for this beam
          self.beam_states[b]["lock"].acquire()
          self.beam_states[b]["state"]     = "Recording"
          self.beam_states[b]["lock"].release()

  ###############################################################################
  # issue_stop_cmd
  def issue_stop_cmd (self, xml):

    # determine which beams this command corresponds to
    for ibeam in range(int(xml['obs_cmd']['beam_configuration']['nbeam'])):
      if xml['obs_cmd']['beam_configuration']['beam_state_' + str(ibeam)]['#text'] == "on":
        b = xml['obs_cmd']['beam_configuration']['beam_state_' + str(ibeam)]['@name']
        if b in self.beam_states.keys():

          self.log(1, "issue_stop_cmd: beam=" + b)
          obs = {}

          self.beam_states[b]["lock"].acquire()
          self.beam_states[b]["state"] = "Stopping"
          obs["COMMAND"] = "STOP"
          if self.beam_states[b]["utc_stop"] == None:
            self.beam_states[b]["utc_stop"] = times.getUTCTime()
          obs["UTC_STOP"] = self.beam_states[b]["utc_stop"]
          self.beam_states[b]["lock"].release()

          # convert to a single ascii string
          obs_header = Config.writeDictToString (obs)

          # work out which streams correspond to these beams
          for istream in range(int(self.cfg["NUM_STREAM"])):
            (host, beam_idx, subband) = self.cfg["STREAM_"+str(istream)].split(":")
            beam = self.cfg["BEAM_" + beam_idx]
            self.log(2, "issue_stop_cmd: host="+host+" beam="+beam+" subband="+subband)

            # connect to streams for this beam only
            if beam == b:

              # control port the this recv stream 
              ctrl_port = int(self.cfg["STREAM_CTRL_PORT"]) + istream

              # connect to recv agent and provide observation configuration
              self.log(3, "issue_stop_cmd: openSocket("+host+","+str(ctrl_port)+")")
              sock = sockets.openSocket (DL, host, ctrl_port, 1)
              if sock:
                self.log(3, "issue_stop_cmd: sending obs_header")
                sock.send(obs_header)
                self.log(3, "issue_stop_cmd: command sent")
                sock.close()
                self.log(3, "issue_stop_cmd: socket closed")

              # connect to spip_gen and issue stop command for UTC
              # assumes gen host is the same as the recv host!
              # gen_port = int(self.cfg["STREAM_GEN_PORT"]) + istream
              # sock = sockets.openSocket (DL, host, gen_port, 1)
              # if sock:
              #   sock.send(obs_header)
              #  sock.close()

          # update the dict of observing info for this beam
          self.beam_states[b]["lock"].acquire()
          self.beam_states[b]["state"] = "Idle"
          self.beam_states[b]["lock"].release()


class TCSServerDaemon (TCSDaemon, ServerBased):

  def __init__ (self, name):
    TCSDaemon.__init__(self,name, "-1")
    ServerBased.__init__(self, self.cfg)
    self.interface_port = int(self.cfg["TCS_INTERFACE_PORT"])
    self.report_port = int(self.cfg["TCS_REPORT_PORT"])

    # beam_states maintains info about last observation for beam
    for i in range(int(self.cfg["NUM_BEAM"])):
      b = self.cfg["BEAM_"+str(i)]
      self.beam_states[b] = {}
      self.beam_states[b]["source"] = ""
      self.beam_states[b]["ra"] = ""
      self.beam_states[b]["dec"] = ""
      self.beam_states[b]["pid"] = ""
      self.beam_states[b]["observer"] = ""
      self.beam_states[b]["utc_start"] = None
      self.beam_states[b]["utc_stop"] = None
      self.beam_states[b]["tobs"] = ""
      self.beam_states[b]["adc_sync_time"] = ""
      self.beam_states[b]["mode"] = ""
      self.beam_states[b]["state"] = "Idle"
      self.beam_states[b]["lock"] = threading.Lock()

class TCSBeamDaemon (TCSDaemon, BeamBased):

  def __init__ (self, name, id):
    TCSDaemon.__init__(self, name, str(id))
    BeamBased.__init__(self, str(id), self.cfg)
    self.interface_port = int(self.cfg["TCS_INTERFACE_PORT"]) + int(id)
    self.report_port = int(self.cfg["TCS_REPORT_PORT"]) + int(id)

    b = self.cfg["BEAM_"+str(id)]
    self.beam_states[b] = {}
    self.beam_states[b]["source"] = ""
    self.beam_states[b]["ra"] = ""
    self.beam_states[b]["dec"] = ""
    self.beam_states[b]["pid"] = ""
    self.beam_states[b]["observer"] = ""
    self.beam_states[b]["utc_start"] = None
    self.beam_states[b]["utc_stop"] = None
    self.beam_states[b]["tobs"] = ""
    self.beam_states[b]["adc_sync_time"] = ""
    self.beam_states[b]["mode"] = ""
    self.beam_states[b]["state"] = "Idle"
    self.beam_states[b]["lock"] = threading.Lock()

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
    script = TCSServerDaemon ("spip_tcs")
    beam_id = 0
  else:
    script = TCSBeamDaemon ("spip_tcs", beam_id)

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
