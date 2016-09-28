#!/usr/bin/env python

###############################################################################
#  
#     Copyright (C) 2015 by Andrew Jameson
#     Licensed under the Academic Free License version 2.1
# 
###############################################################################

from katcp import DeviceServer, Sensor, ProtocolFlags, AsyncReply
from katcp.kattypes import (Str, Int, Float, Bool, Timestamp, Discrete,
                            request, return_reply)

import logging
import tornado.gen
from katportalclient import KATPortalClient

import os, threading, sys, socket, select, signal, traceback, xmltodict
import errno, time, random, re

from xmltodict import parse
from xml.parsers.expat import ExpatError

from spip.daemons.bases import ServerBased,BeamBased
from spip.daemons.daemon import Daemon
from spip.log_socket import LogSocket
from spip.utils import sockets,times
from spip.utils import catalog
from spip.threads.reporting_thread import ReportingThread
from spip.threads.socketed_thread import SocketedThread

DAEMONIZE = True
DL = 1

###############################################################
# PubSub daemon
class PubSubThread (threading.Thread):

  def __init__ (self, script, id):
    threading.Thread.__init__(self)
    self.script = script
   
    self.script.log(2, "PubSubThread.__init__()")
   
    self.metadata_server = "ws://portal.mkat.devlab.camlab.kat.ac.za/katmetadata/subarray-1/custom/websocket"

    self.logger = logging.getLogger('katportalclient.example') 
    self.logger.setLevel(logging.INFO)

    self.io_loop = tornado.ioloop.IOLoop.current()
    self.io_loop.add_callback (self.connect, self.logger)

    prefix = "subarray_" + str(int(id+1)) + "_script"

    self.subs = ['cbf.*.bandwidth', 'cbf.*.centerfrequency', 'cbf.*.channels']
    self.subs.append ('cbf.synchronisation.epoch')
    self.subs.append ('script.observer')
    self.subs.append ('script.target')
    self.subs.append ('product')

    self.adc_start_re = re.compile ("data_1_cbf_synchronisation_epoch")
    self.bandwidth_re = re.compile ("data_1_cbf_[a-zA-Z0-9]*_bandwidth")
    self.centerfreq_re = re.compile ("data_1_cbf_[a-zA-Z0-9]*_centerfrequency")
    self.channels_re = re.compile ("data_1_cbf_[a-zA-Z0-9]*_channels")
    self.target_re = re.compile ("subarray_1_script_target")
    self.ra_re = re.compile ("subarray_1_script_ra")
    self.dec_re = re.compile ("subarray_1_script_dec")
    self.observer_re = re.compile ("subarray_1_script_observer")
    self.tsubint_re = re.compile ("subarray_1_script_tsubint")

  def run (self):
    self.script.log(2, "PubSubThread.run()")
    self.script.log(3, "PubSubThread.io_loop starting...")
    self.io_loop.start()

  def join (self):
    self.script.log(2, "PubSubThread.join()")
    self.stop()

  def stop (self):
    self.script.log(2, "PubSubThread.stop()")
    self.io_loop.stop()
    return

  @tornado.gen.coroutine
  def connect (self, logger):
    self.script.log(2, "PubSubThread.connect()")
    self.ws_client = KATPortalClient(self.metadata_server, self.on_update_callback, logger=logger)
    yield self.ws_client.connect()
    result = yield self.ws_client.subscribe('ptuse_test')

    list = ['cbf.*.bandwidth', 'cbf.*.centerfrequency', 'cbf.*.channels']
    list.append ('cbf.synchronisation.epoch')
    list.append ('subarray_1')
    list.append ('script.observer')
    list.append ('script.target')
    list.append ('product')

    result = yield self.ws_client.set_sampling_strategies(
        'ptuse_test', list, 'period 10.0')

  def on_update_callback (self, msg):

    # determine how to know which beam this corresponds to!
    ibeam = 0
    beam_name = self.script.cfg["BEAM_" + str(ibeam)]

    self.script.beam_configs[beam_name]["lock"].acquire()

    self.update_config (self.script.beam_configs[beam_name],  msg)

    self.script.beam_configs[beam_name]["lock"].release()

    if self.script.quit_event.isSet():
      self.script.log(1, "PubSubThread::on_update_callback: self.stop()")
      self.stop()

  def update_config (self, bcfg, msg):

    # ignore empty messages
    if msg == []: 
      return

    status = msg["msg_data"]["status"]
    value = msg["msg_data"]["value"]
    name = msg["msg_data"]["name"]

    self.script.log(2, "PubSubThread::update_config " + name + "=" + str(value))

    # get the ADC_SYNC_TIME
    if self.adc_start_re.match (name):
      bcfg["ADC_SYNC_TIME"] = str(int(value))
      self.script.log(1, "PubSubThread::update_config ADC_SYNC_TIME=" + str(value) + " from " + name)

    elif self.target_re.match (name):
      bcfg["SOURCE"] = value
      self.script.log(1, "PubSubThread::update_config SOURCE=" + str(value) + " from " + name)

    # TODO check if exists already
    elif self.ra_re.match (name):
      bcfg["RA"] = value
      self.script.log(1, "PubSubThread::update_config RA=" + str(value) + " from " + name)

    # TODO check if exists already
    elif self.dec_re.match (name):
      bcfg["DEC"] = value
      self.script.log(1, "PubSubThread::update_config DEC=" + str(value) + " from " + name)

    elif self.observer_re.match (name):
      bcfg["OBSERVER"] = value
      self.script.log(1, "PubSubThread::update_config OBSERVER=" + str(value) + " from " + name)

    elif self.bandwidth_re.match (name):
      self.script.log(1, "PubSubThread::update_config BANDWIDTH=" + str(value) + " from " + name)

    elif self.centerfreq_re.match (name):
      self.script.log(1, "PubSubThread::update_config CFREQ=" + str(value) + " from " + name)

    elif self.tsubint_re.match (name):
      bcfg["TSUBINT"] = value
      self.script.log(1, "PubSubThread::update_config TSUBINT=" + str(value) + " from " + name)

    self.script.log(3, "PubSubThread::update_config done")

class PubSubSimThread(SocketedThread):

  def __init__ (self, script, id):
    self.script = script
    host = sockets.getHostNameShort()
    port = int(script.cfg["MEERKAT_PUBSUB_PORT"]) + int(id)
    SocketedThread.__init__(self, script, host, port)

  def run (self):
    SocketedThread.run(self)

  def process_message_on_handle (self, handle):

    # TODO here we will need to process the JSON Websockets 
    # formatted data as per the CAM specification which is 
    # TBD. For now, assume SPIP Test Interface format
    retain = False
    raw = handle.recv(4096)
    message = raw.strip()
    self.script.log (2, "PubSubSimThread: message="+str(message))

    if len(message) == 0:
      handle.close()
      for i, x in enumerate(self.can_read):
        if (x == handle):
          del self.can_read[i]

    else:
      try:
        xml = parse(message)
      except ExpatError as e:
        handle.send ("<xml>Malformed XML message</xml>\r\n")
        handle.close()
        for i, x in enumerate(self.can_read):
          if (x == handle):
            del self.can_read[i]

      # for each bema that the messages corresponds to
      for ibeam in range(int(xml['obs_cmd']['beam_configuration']['nbeam'])):

        # check that the beam is active for the message
        if xml['obs_cmd']['beam_configuration']['beam_state_' + str(ibeam)] == "on":

          # get the name of the beam
          beam_name = self.script.cfg["BEAM_" + str(ibeam)]

          self.script.beam_configs[beam_name]["lock"].acquire()

          self.script.beam_configs[beam_name]["SOURCE"] = xml['obs_cmd']['source_parameters']['name']['#text']
          self.script.beam_configs[beam_name]["RA"] = xml['obs_cmd']['source_parameters']['ra']['#text']
          self.script.beam_configs[beam_name]["DEC"] = xml['obs_cmd']['source_parameters']['dec']['#text']

          self.script.beam_configs[beam_name]["OBSERVER"] = str(xml['obs_cmd']['observation_parameters']['observer'])
          self.script.beam_configs[beam_name]["PID"] = str(xml['obs_cmd']['observation_parameters']['project_id'])
          self.script.beam_configs[beam_name]["MODE"] = xml['obs_cmd']['observation_parameters']['mode']
          self.script.beam_configs[beam_name]["PROC_FILE"] = str(xml['obs_cmd']['observation_parameters']['processing_file'])

          #self.script.beam_configs[beam_name]["UTC_START"] = xml['obs_cmd']['observation_parameters']['utc_start']
          self.script.beam_configs[beam_name]["OBS_OFFSET"] = "0"
          self.script.beam_configs[beam_name]["TOBS"] = xml['obs_cmd']['observation_parameters']['tobs']

          self.script.beam_configs[beam_name]["ADC_SYNC_TIME"] =  xml['obs_cmd']['instrument_parameters']['adc_sync_time']
          self.script.beam_configs[beam_name]["PERFORM_FOLD"] = "1"
          self.script.beam_configs[beam_name]["PERFORM_SEARCH"] = "0"
          self.script.beam_configs[beam_name]["PERFORM_TRANS"] = "0"

          self.script.beam_configs[beam_name]["lock"].release()

      response = "ok"
      reply = "<?xml version='1.0' encoding='ISO-8859-1'?>" + \
              "<pubsub_response>" + response + "</pubsub_response>"

      self.script.log(3, "<- " + str(xml))
      handle.send (reply)
      if not retain:
        self.script.log (2, "PubSubSimThread: closing connection")
        handle.close()
        for i, x in enumerate(self.can_read):
          if (x == handle):
                del self.can_read[i]





###############################################################
# KATCP daemon
class KATCPDaemon(Daemon):

  def __init__ (self, name, id):
    Daemon.__init__(self, name, str(id))
    self.beam_states = {}
    self.beam_configs = {}
    self.tcs_hosts = {}
    self.tcs_ports = {}
    self.beams = []
    self.hosts = []
    self.katcp = []
    self.snr = 0
    self.stddev = 0

    # get a list of all LMCs
    self.lmcs = []
    for i in range(int(self.cfg["NUM_STREAM"])):
      (host, beam, subband) = self.cfg["STREAM_" + str(i)].split(":")
      lmc = host +  ":" + self.cfg["LMC_PORT"]
      if not (lmc in self.lmcs):
        self.lmcs.append(lmc)
        self.hosts.append(host)
    lmc = self.cfg["SERVER_HOST"] + ":" + self.cfg["LMC_PORT"]
    if not (lmc in self.lmcs):
      self.lmcs.append(lmc)
      self.hosts.append(self.cfg["SERVER_HOST"])
    self.lmc_cmd = "<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?>" + \
                   "<lmc_cmd>" + \
                   "<requestor>katcp daemon</requestor>" + \
                   "<command>host_status</command>" + \
                   "</lmc_cmd>"

    self.cpu_temp_pattern  = re.compile("cpu[0-9]+_temp")
    self.fan_speed_pattern = re.compile("fan[0-9,a-z]+")
    self.power_supply_pattern = re.compile("ps[0-9]+_status")

    # get a list of all the repacking scripts
    self.repacks = []
    if self.cfg["INDEPENDENT_BEAMS"] == "true":
      for i in range(int(self.cfg["NUM_STREAM"])):
        (host, beam, subband) = self.cfg["STREAM_" + str(i)].split(":")
        repack = host + ":" + str(int(self.cfg["STREAM_REPACK_PORT"]) + i)
        self.repacks.append(repack)
    else:
      repack = self.cfg["SERVER_HOST"] + ":" + self.cfg["STREAM_REPACK_PORT"]
      self.repacks.append(repack)
    self.repack_cmd = "<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?>" + \
                      "<repack_request>" + \
                      "<requestor>katcp daemon</requestor>" + \
                      "<type>state</type>" + \
                      "</repack_request>"

  # reset the sepcified beam configuration
  def reset_beam_config (self, bcfg):
    bcfg["lock"].acquire()
    bcfg["SOURCE"] = "J0835-4510"
    bcfg["RA"] = "None"
    bcfg["DEC"] = "None"
    bcfg["PID"] = "None"
    bcfg["OBSERVER"] = "None"
    bcfg["TOBS"] = "None"
    bcfg["MODE"] = "PSR"
    bcfg["PROC_FILE"] = "None"
    bcfg["ADC_SYNC_TIME"] = "0"
    bcfg["PERFORM_FOLD"] = "1"
    bcfg["PERFORM_SEARCH"] = "0"
    bcfg["PERFORM_TRANS"] = "0"
    bcfg["lock"].release()


  #############################################################################
  # configure fixed sensors (for lifetime of script)
  def set_katcp (self, server):
    self.katcp = server
    r = ""

    if len(self.beams) > 1:
      r = ",".join(str(b) for b in self.beams)
    elif len(self.beams) == 1:
      r = str(self.beams[0])
    else:
      self.log(-2, "set_katcp: no beams configured")
    self.katcp._beam_list.set_value (r)

    r = ""
    if len(self.hosts) > 1:
      r = ",".join(str(b) for b in self.beams)
    elif len(self.hosts) == 1:
      r = str(self.hosts[0])
    else:
      self.log(-2, "set_katcp: no hosts configured")
    self.katcp._host_list.set_value (r)

  #############################################################################
  # script core
  def main (self, id):

    # connect to the various scripts running to collect the information
    # to be provided by the KATCPServer instance as sensors

    time.sleep(2)

    while not self.quit_event.isSet():

      # the primary function of the KATCPDaemon is to update the 
      # sensors in the DeviceServer periodically

      # TODO compute overall device status
      self.katcp._device_status.set_value("ok")

      # connect to SPIP_LMC to retreive temperature information
      for lmc in self.lmcs:
        if self.quit_event.isSet():
          return
        (host, port) = lmc.split(":")
        self.log(2, "KATCPDaemon::main openSocket("+host+","+port+")")
        try:
          sock = sockets.openSocket (DL, host, int(port), 1)
          if sock:
            sock.send(self.lmc_cmd)
            lmc_reply = sock.recv (65536)
            xml = xmltodict.parse(lmc_reply)
            sock.close()

            if self.quit_event.isSet():
              return
            self.log(2, "KATCPDaemon::main update_lmc_sensors("+host+",[xml])")
            self.update_lmc_sensors(host, xml)

        except socket.error as e:
          if e.errno == errno.ECONNRESET:
            self.log(1, "lmc connection was unexpectedly closed")
            sock.close()

      # connect to SPIP_REPACK to retrieve Pulsar SNR performance
      for repack in self.repacks:
        if self.quit_event.isSet():
          return
        (host, port) = repack.split(":")
        try:
          sock = sockets.openSocket (DL, host, int(port), 1)
          if sock:
            sock.send (self.repack_cmd)
            repack_reply = sock.recv (65536)
            xml = xmltodict.parse(repack_reply)
            sock.close()

            if self.quit_event.isSet():
              return
            self.log(2, "KATCPDaemon::main update_repack_sensors("+host+",[xml])")
            self.update_repack_sensors(host, xml)

        except socket.error as e:
          if e.errno == errno.ECONNRESET:
            self.log(1, "repack connection was unexpectedly closed")
            sock.close()

      to_sleep = 5
      while not self.quit_event.isSet() and to_sleep > 0:
        to_sleep -= 1
        time.sleep (1) 

      if not self.katcp.running():
        self.log (-2, "KATCP server was not running, exiting")
        self.quit_event.set()

  #############################################################################
  # return valid XML configuration for specified beam
  def get_xml_config_for_beams (self, b):

    xml =  "<?xml version='1.0' encoding='ISO-8859-1'?>"
    xml += "<obs_cmd>"
    xml +=   "<command>configure</command>"
    xml +=   "<beam_configuration>"
    xml +=     "<nbeam>" + str(len(self.beams)) + "</nbeam>"
    for i in range(len(self.beams)):
      if self.beams[i] == b:
        xml +=     "<beam_state_" + str(i) + " name='" + self.beams[i] + "'>on</beam_state_" + str(i) + ">"
      else:
        xml +=     "<beam_state_" + str(i) + " name='" + self.beams[i] + "'>off</beam_state_" + str(i) + ">"
    xml +=   "</beam_configuration>"

    xml +=   "<source_parameters>"
    xml +=     "<name epoch='J2000'>" + self.beam_configs[b]["SOURCE"] + "</name>"

    ra = self.beam_configs[b]["RA"]
    if ra == "None":
      (reply, message) = catalog.get_psrcat_param (self.beam_configs[b]["SOURCE"], "raj")
      if reply == "ok":
        ra = message

    dec = self.beam_configs[b]["DEC"]
    if dec == "None":
      (reply, message) = catalog.get_psrcat_param (self.beam_configs[b]["SOURCE"], "decj")
      if reply == "ok":
        dec = message

    xml +=     "<ra units='hh:mm:ss'>" + ra + "</ra>"
    xml +=     "<dec units='hh:mm:ss'>" + dec+ "</dec>"
    xml +=   "</source_parameters>"

    xml +=   "<observation_parameters>"
    xml +=     "<observer>" + self.beam_configs[b]["OBSERVER"] + "</observer>"
    xml +=     "<project_id>" + self.beam_configs[b]["PID"] + "</project_id>"
    xml +=     "<tobs>" + self.beam_configs[b]["TOBS"] + "</tobs>"
    xml +=     "<mode>" + self.beam_configs[b]["MODE"] + "</mode>"
    xml +=     "<processing_file>" + self.beam_configs[b]["PROC_FILE"] + "</processing_file>"
    xml +=     "<utc_start></utc_start>"
    xml +=     "<utc_stop></utc_stop>"
    xml +=   "</observation_parameters>"

    xml +=   "<instrument_parameters>"
    xml +=     "<adc_sync_time>" + self.beam_configs[b]["ADC_SYNC_TIME"] + "</adc_sync_time>"
    xml +=   "</instrument_parameters>"

    xml += "</obs_cmd>"

    return xml

  def get_xml_start_cmd (self, b):

    xml  = "<?xml version='1.0' encoding='ISO-8859-1'?>"
    xml += "<obs_cmd>"
    xml += "<command>start</command>"
    xml +=   "<beam_configuration>"
    xml +=     "<nbeam>" + str(len(self.beams)) + "</nbeam>"
    for i in range(len(self.beams)):
      if self.beams[i] == b:
        xml +=     "<beam_state_" + str(i) + " name='" + self.beams[i] + "'>on</beam_state_" + str(i) + ">"
      else:
        xml +=     "<beam_state_" + str(i) + " name='" + self.beams[i] + "'>off</beam_state_" + str(i) + ">"
    xml +=   "</beam_configuration>"
    xml +=   "<observation_parameters>"
    xml +=     "<utc_start></utc_start>"
    xml +=   "</observation_parameters>"
    xml += "</obs_cmd>"

    return xml

  def get_xml_stop_cmd (self, b):

    xml  = "<?xml version='1.0' encoding='ISO-8859-1'?>"
    xml += "<obs_cmd>"
    xml += "<command>stop</command>"
    xml +=   "<beam_configuration>"
    xml +=     "<nbeam>" + str(len(self.beams)) + "</nbeam>"
    for i in range(len(self.beams)):
      if self.beams[i] == b:
        xml +=     "<beam_state_" + str(i) + " name='" + self.beams[i] + "'>on</beam_state_" + str(i) + ">"
      else:
        xml +=     "<beam_state_" + str(i) + " name='" + self.beams[i] + "'>off</beam_state_" + str(i) + ">"
    xml +=   "</beam_configuration>"
    xml +=   "<observation_parameters>"
    xml +=     "<utc_stop></utc_stop>"
    xml +=   "</observation_parameters>"
    xml += "</obs_cmd>"

    return xml


  def update_lmc_sensors (self, host, xml):

    self.katcp._host_sensors[host]["disk_size"].set_value (float(xml["lmc_reply"]["disk"]["size"]["#text"]))
    self.katcp._host_sensors[host]["disk_available"].set_value (float(xml["lmc_reply"]["disk"]["available"]["#text"]))

    self.katcp._host_sensors[host]["num_cores"].set_value (int(xml["lmc_reply"]["system_load"]["@ncore"]))
    self.katcp._host_sensors[host]["load1"].set_value (float(xml["lmc_reply"]["system_load"]["load1"]))
    self.katcp._host_sensors[host]["load5"].set_value (float(xml["lmc_reply"]["system_load"]["load5"]))
    self.katcp._host_sensors[host]["load15"].set_value (float(xml["lmc_reply"]["system_load"]["load15"]))

    if not xml["lmc_reply"]["sensors"] == None:
      for sensor in xml["lmc_reply"]["sensors"]["metric"]:

        name = sensor["@name"]
        value = sensor["#text"]

        if name == "system_temp":
          self.katcp._host_sensors[host][name].set_value (float(value))
        elif (self.cpu_temp_pattern.match(name)):
          (cpu, junk) = name.split("_")
          self.katcp._host_sensors[host][name].set_value (float(value))
        elif (self.fan_speed_pattern.match(name)):
          self.katcp._host_sensors[host][name].set_value (float(value))
        elif (self.power_supply_pattern.match(name)):
          self.katcp._host_sensors[host][name].set_value (value == "ok")

  def update_repack_sensors (self, host, xml):

    for beam in xml["repack_state"].items():
      # each beam is a tuple with [0]=beam and [1]=XML
      beam_name = beam[1]["@name"]
      active    = beam[1]["@active"]

      self.log (2, "KATCPDaemon::update_repack_sensors beam="+beam_name+" active="+active)
      if active == "True":
        source = beam[1]["source"]["name"]["#text"]
        self.log (2, "KATCPDaemon::update_repack_sensors source="+source)

        start = beam[1]["observation"]["start"]["#text"]
        integrated = beam[1]["observation"]["integrated"]["#text"]
        snr = beam[1]["observation"]["snr"]
        self.log (2, "KATCPDaemon::update_repack_sensors start="+start+" length="+integrated+" snr="+snr)

        self.katcp._beam_sensors[beam_name]["observing"].set_value (1)
        self.katcp._beam_sensors[beam_name]["snr"].set_value (float(snr))
        self.katcp._beam_sensors[beam_name]["integrated"].set_value (float(integrated))

      else:
        self.katcp._beam_sensors[beam_name]["observing"].set_value (0)
        self.katcp._beam_sensors[beam_name]["snr"].set_value (0)
        self.katcp._beam_sensors[beam_name]["integrated"].set_value (0)
        

    return ("ok")

  def get_beam_list (self):
    if len(self.beams) > 1:
      return ("ok", ",".join(str(b) for b in self.beams))
    elif len(self.beams) == 1:
      return ("ok", str(self.beams[0]))
    else:
      return ("fail", "no beams configured")

  def get_SNR (self, b):
    if b in self.beams:  
      self.beam_states[b]["SNR"] += 1
      return ("ok", self.beam_states[b]["SNR"])
    else:
      return ("fail", 0)

  def get_power(self, b):
    if b in self.beams:
      self.beam_states[b]["POWER"] += 1
      return ("ok", self.beam_states[b]["POWER"])
    else:
      return ("fail", 0)

class KATCPServerDaemon (KATCPDaemon, ServerBased):

  def __init__ (self, name):
    KATCPDaemon.__init__(self,name, "-1")
    ServerBased.__init__(self, self.cfg)

    # when only a single KATCP instance, maintain SNRs
    # for all beams
    for i in range(int(self.cfg["NUM_BEAM"])):
      b = self.cfg["BEAM_" + str(i)]
      self.beam_states[b] = {}
      self.beam_states[b]["NAME"] = b
      self.beam_states[b]["SNR"] = 0
      self.beam_states[b]["POWER"] = 0

      self.beam_configs[b] = {}
      self.beam_configs[b]["lock"] = threading.Lock()
      self.reset_beam_config (self.beam_configs[b])

      # if each beam is used independently of the others
      if self.cfg["INDEPENDENT_BEAMS"] == "true":
        # find the lowest number stream for the beam
        lowest_stream = int(self.cfg["NUM_STREAM"])
        for j in range(int(self.cfg["NUM_STREAM"])):
          (host, beam, subband) = self.cfg["STREAM_" + str(j)].split(":")
          if (beam == b and j < lowest_stream):
            lowest_stream = j
            self.tcs_hosts[b] = host
            self.tcs_ports[b] = int(self.cfg["TCS_INTERFACE_PORT"]) + i
        if lowest_stream == int(self.cfg["NUM_STREAM"]):
          self.log(-2, "KATCPServerDaemon::__init__ could not match beam to stream")
      else:
        self.tcs_hosts[b] = self.cfg["TCS_INTERFACE_HOST"]
        self.tcs_ports[b] = self.cfg["TCS_INTERFACE_PORT"]

    self.beams = self.beam_states.keys()

class KATCPBeamDaemon (KATCPDaemon, BeamBased):

  def __init__ (self, name, id):
    KATCPDaemon.__init__(self, name, str(id))
    BeamBased.__init__(self, str(id), self.cfg)

    b = id + 1

    self.beam_states[b] = {}
    self.beam_states[b]["NAME"] = self.cfg["BEAM_"+str(id)]
    self.beam_states[b]["SNR"] = 0
    self.beam_states[b]["POWER"] = 0
    self.beams = self.beam_states.keys()

    self.beam_configs[b] = {}
    self.beam_configs[b]["lock"] = threading.Lock()
    self.reset_beam_config (self.beam_configs[b])

    # if each beam is used independently of the others
    if self.cfg["INDEPENDENT_BEAMS"] == "true":
      self.tcs_hosts[b] = self.cfg["TCS_INTERFACE_HOST_" + str(id)]
      self.tcs_ports[b] = self.cfg["TCS_INTERFACE_PORT_" + str(id)]
    else:
      self.tcs_hosts[b] = self.cfg["TCS_INTERFACE_HOST"]
      self.tcs_ports[b] = self.cfg["TCS_INTERFACE_PORT"]
  

##############################################################
# Actual KATCP server implementation
class KATCPServer (DeviceServer):

    VERSION_INFO = ("ptuse-api", 1, 0)
    BUILD_INFO = ("ptuse-implementation", 0, 1, "")

    # Optionally set the KATCP protocol version and features. Defaults to
    # the latest implemented version of KATCP, with all supported optional
    # features
    PROTOCOL_INFO = ProtocolFlags(5, 0, set([
      ProtocolFlags.MULTI_CLIENT,
      ProtocolFlags.MESSAGE_IDS,
    ]))

    def __init__ (self, server_host, server_port, script):
      self.script = script
      self._host_sensors = {}
      self._beam_sensors = {}
      self._data_products = {}
      self.configured_beams = 0
      self.available_beams = int(self.script.cfg["NUM_BEAM"])

      self.script.log(2, "KATCPServer::__init__ starting DeviceServer on " + server_host + ":" + str(server_port))
      DeviceServer.__init__(self, server_host, server_port)

    DEVICE_STATUSES = ["ok", "degraded", "fail"]

    def setup_sensors(self):
      """Setup server sensors."""
      self.script.log(2, "KATCPServer::setup_sensors()")

      self._device_status = Sensor.discrete("device-status",
        description="Status of entire system",
        params=self.DEVICE_STATUSES,
        default="ok")
      self.add_sensor(self._device_status)

      self._beam_list = Sensor.string("beam-list",
        description="list of configured beams",
        unit="",
        default="")
      self.add_sensor(self._beam_list)

      # setup host based sensors   
      self._host_list= Sensor.string("host-list",
        description="list of configured servers",
        unit="",
        default="")
      self.add_sensor(self._host_list)

      self.script.log(2, "KATCPServer::setup_sensors lmcs="+str(self.script.lmcs))
      for lmc in self.script.lmcs:
        (host, port) = lmc.split(":")
        self.setup_sensors_host (host, port)

      self.script.log(2, "KATCPServer::setup_sensors beams="+str(self.script.beams))
      for b in self.script.beams:
        self.setup_sensors_beam (b)

    # add sensors based on the reply from the specified host
    def setup_sensors_host (self, host, port):

      self.script.log(2, "KATCPServer::setup_sensors_host ("+host+","+port+")")
      sock = sockets.openSocket (DL, host, int(port), 1)

      if sock:
        self.script.log(2, "KATCPServer::setup_sensors_host sock.send(" + self.script.lmc_cmd + ")") 
        sock.send (self.script.lmc_cmd + "\r\n")
        lmc_reply = sock.recv (65536)
        sock.close()
        xml = xmltodict.parse(lmc_reply)

        self._host_sensors[host] = {}
        self._host_sensors[host]["sensors"] = {}

        # Disk sensors
        self.script.log(2, "KATCPServer::setup_sensors_host configuring disk sensors")
        disk_prefix = host+".disk"
        self._host_sensors[host]["disk_size"] = Sensor.float(disk_prefix+".size",
          description=host+": disk size",
          unit="MB",
          params=[8192,1e9],
          default=0)
        self._host_sensors[host]["disk_available"] = Sensor.float(disk_prefix+".available",
          description=host+": disk available space",
          unit="MB",
          params=[1024,1e9],
          default=0)
        self.add_sensor(self._host_sensors[host]["disk_size"])
        self.add_sensor(self._host_sensors[host]["disk_available"])

        # Server Load sensors
        self.script.log(2, "KATCPServer::setup_sensors_host configuring load sensors")
        self._host_sensors[host]["num_cores"] = Sensor.integer (host+".num_cores",
          description=host+": disk available space",
          unit="MB",
          params=[1,64],
          default=0)

        self._host_sensors[host]["load1"] = Sensor.float(host+".load.1min",
          description=host+": 1 minute load ",
          unit="",
          default=0)

        self._host_sensors[host]["load5"] = Sensor.float(host+".load.5min",
          description=host+": 5 minute load ",
          unit="",
          default=0)
        
        self._host_sensors[host]["load15"] = Sensor.float(host+".load.15min",
          description=host+": 15 minute load ",
          unit="",
          default=0)

        self.add_sensor(self._host_sensors[host]["num_cores"])
        self.add_sensor(self._host_sensors[host]["load1"])
        self.add_sensor(self._host_sensors[host]["load5"])
        self.add_sensor(self._host_sensors[host]["load15"])

        cpu_temp_pattern  = re.compile("cpu[0-9]+_temp")
        fan_speed_pattern = re.compile("fan[0-9,a-z]+")
        power_supply_pattern = re.compile("ps[0-9]+_status")
          
        self.script.log(2, "KATCPServer::setup_sensors_host configuring other metrics")

        if not xml["lmc_reply"]["sensors"] == None:

          for sensor in xml["lmc_reply"]["sensors"]["metric"]:
            name = sensor["@name"]
            if name == "system_temp":
              self._host_sensors[host][name] = Sensor.float((host+".system_temp"),
                description=host+": system temperature",
                unit="C",
                params=[-20,150],
                default=0)
              self.add_sensor(self._host_sensors[host][name])

            if cpu_temp_pattern.match(name):
              (cpu, junk) = name.split("_")
              self._host_sensors[host][name] = Sensor.float((host+"." + name),
                description=host+": "+ cpu +" temperature",
                unit="C",
                params=[-20,150],
                default=0)
              self.add_sensor(self._host_sensors[host][name])

            if fan_speed_pattern.match(name):
              self._host_sensors[host][name] = Sensor.float((host+"." + name),
                description=host+": "+name+" speed",
                unit="RPM",
                params=[0,20000],
                default=0)
              self.add_sensor(self._host_sensors[host][name])

            if power_supply_pattern.match(name):
              self._host_sensors[host][name] = Sensor.boolean((host+"." + name),
                description=host+": "+name,
                unit="",
                default=0)
              self.add_sensor(self._host_sensors[host][name])

            # TODO consider adding power supply sensors: e.g.
            #   device-status-kronos1-powersupply1
            #   device-status-kronos1-powersupply2
            #   device-status-kronos2-powersupply1
            #   device-status-kronos2-powersupply2

            # TODO consider adding raid/disk sensors: e.g.
            #   device-status-<host>-raid
            #   device-status-<host>-raid-disk1
            #   device-status-<host>-raid-disk2

          self.script.log(2, "KATCPServer::setup_sensors_host done!")

        else:
          self.script.log(2, "KATCPServer::setup_sensors_host no sensors found")

      else:
        self.script.log(-2, "KATCPServer::setup_sensors_host: could not connect to LMC")

    # setup sensors for each beam
    def setup_sensors_beam (self, beam):

      b = str(beam)
      self._beam_sensors[b] = {}

      self.script.log(2, "KATCPServer::setup_sensors_beam ="+b)

      self._beam_sensors[b]["observing"] = Sensor.boolean("beam"+b+".observing",
        description="Beam " + b + " is observing",
        unit="",
        default=0)
      self.add_sensor(self._beam_sensors[b]["observing"])

      self._beam_sensors[b]["snr"] = Sensor.float("beam"+b+".snr",
        description="SNR of Beam "+b,
        unit="",
        params=[0,1e9],
        default=0)
      self.add_sensor(self._beam_sensors[b]["snr"])

      self._beam_sensors[b]["power"] = Sensor.float("beam"+b+".power",
        description="SNR of Beam "+b,
        unit="",
        default=0)
      self.add_sensor(self._beam_sensors[b]["power"])

      self._beam_sensors[b]["integrated"] = Sensor.float("beam"+b+".integrated",
        description="Length of integration for Beam "+b,
        unit="",
        default=0)
      self.add_sensor(self._beam_sensors[b]["integrated"])

    @request()
    @return_reply(Str())
    def request_beam_list(self, req):
      """Return the list of configured beams."""
      state, r = script.get_beam_list()
      self._beam_list.set_value(r)
      return (state, r)

    @request()
    @return_reply(Str())
    def request_host_list(self, req):
      """Return the list of configured servers."""
      r = self._host_list.value()
      return ("ok", r)

    @request(Int())
    @return_reply(Float())
    def request_snr(self, req, beam):
      """Return the SNR of the specifed beam."""
      state, r = self.script.get_SNR(beam)
      if state == "ok":
        self._snr_results[beam].set_value(r)
        return ("ok", r)
      else:
        return ("fail", r)

    @request(Int())
    @return_reply(Float())
    def request_power(self, req, beam):
      """Return the standard deviation of the 8-bit power levels of the specified beam."""
      state, r = script.get_power(beam)
      if state == "ok":
        self._power_results[beam].set_value(r)
        return ("ok", r)
      else:
        return ("fail", 0)

    @request(Str(), Float())
    @return_reply(Str())
    def request_sync_time (self, req, data_product_id, adc_sync_time):
      """Set the ADC_SYNC_TIME for all beams in the specified data product."""

      if not data_product_id in self._data_products.keys():
        return ("fail", "data product " + str (data_product_id) + " was not configured")

      if len(self._data_products[data_product_id]["beams"]) <= 0:
        return ("fail", "data product " + data_product_id + " had no configured beams")

      for ibeam in self._data_products[data_product_id]['beams']:
        b = self.script.beams[ibeam]
        self.script.beam_configs[b]["ADC_SYNC_TIME"] = str(adc_sync_time)

      return ("ok", "")

    @request(Str(), Str(), Str())
    @return_reply(Str())
    def request_target_start (self, req, data_product_id, beam_id, target_name):
      """Commence data processing on specific data product and beam using target."""
      self.script.log (1, "request_target_start(" + data_product_id + "," + beam_id + "," + target_name+")")
      (result, message) = self.test_beam_valid (data_product_id, beam_id)
      if result == "fail":
        return (result, message)

      # check the pulsar specified is listed in the catalog
      (result, message) = self.test_pulsar_valid (target_name)
      if result != "ok":
        return (result, message)
    
      # set the pulsar name, this should include a check if the pulsar is in the catalog
      self.script.beam_configs[beam_id]["SOURCE"] = target_name

      host = self.script.tcs_hosts[beam_id]
      port = self.script.tcs_ports[beam_id]

      self.script.log (2, "request_target_start: opening socket for beam " + beam_id + " to " + host + ":" + str(port))
      sock = sockets.openSocket (DL, host, int(port), 1)
      if sock:
        xml = self.script.get_xml_config_for_beams (beam_id)
        sock.send(xml + "\r\n")
        reply = sock.recv (65536)

        xml = self.script.get_xml_start_cmd (beam_id)
        sock.send(xml + "\r\n")
        reply = sock.recv (65536)

        sock.close()
        return ("ok", "")
      else:
        return ("fail", "could not connect to TCS")

    @request(Str(), Str())
    @return_reply(Str())
    def request_target_stop (self, req, data_product_id, beam_id):
      """Cease data processing with target_name."""
      self.script.log (1, "request_target_stop(" + data_product_id+","+beam_id+")")

      (result, message) = self.test_beam_valid (data_product_id, beam_id)
      if result == "fail":
        return (result, message)

      self.script.beam_configs[beam_id]["SOURCE"] = ""

      host = self.script.tcs_hosts[beam_id]
      port = self.script.tcs_ports[beam_id]
      sock = sockets.openSocket (DL, host, int(port), 1)
      if sock:
        xml = self.script.get_xml_stop_cmd (beam_id)
        sock.send(xml + "\r\n")
        reply = sock.recv (65536)
        sock.close()
        return ("ok", "")
      else:
        return ("fail", "could not connect to tcs[beam]")

    @request(Str())
    @return_reply(Str())
    def request_capture_init (self, req, data_product_id):
      """Prepare the ingest process for data capture."""
      self.script.log (1, "request_capture_init()")
      if not data_product_id in self._data_products.keys():
        return ("fail", "data product " + str (data_product_id) + " was not configured")
      return ("ok", "")

    @request(Str())
    @return_reply(Str())
    def request_capture_done(self, req, data_product_id):
      """Terminte the ingest process for the specified data_product_id."""
      if not data_product_id in self._data_products.keys():
        return ("fail", "data product " + str (data_product_id) + " was not configured")
      return ("ok", "")

    @return_reply(Str())
    def request_data_product_configure(self, req, msg):
      """Prepare and configure for the reception of the data_product_id."""
      self.script.log (1, "request_data_product_configure() msg=" + str(msg))
      if len(msg.arguments) == 0:
        return ("ok", "configured data products: TBD")

      # the sub-array identifier
      data_product_id = msg.arguments[0]

      if len(msg.arguments) == 1:
        if data_product_id in self._data_products:
          configuration = str(data_product_id) + " " + \
                          str(self._data_products[data_product_id]['antennas']) + " " + \
                          str(self._data_products[data_product_id]['n_channels']) + " " + \
                          str(self._data_products[data_product_id]['dump_rate']) + " " + \
                          str(self._data_products[data_product_id]['n_beams']) + " " + \
                          str(self._data_products[data_product_id]['cbf_source'])
          return ("ok", configuration)
        else:
          return ("fail", "no configuration existed for " + str(data_product_id))

      # The data product is deconfigured, and beams released
      if len(msg.arguments) == 5:
        self.script.log (1, "data_product_configure: deconfiguring " + str(data_product_id))

        if not data_product_id in self._data_products.keys():
          return ("fail", data_product_id + " was not a configured data product")

        # check if the data_product was configured
        if len(self._data_products[data_product_id]['beams']) == 0:
          return ("fail", data_product_id + " had 0 beams configured")

        # update config file for beams to match the cbf_source
        for ibeam in self._data_products[data_product_id]['beams']:
          b = self.script.beams[ibeam]
          for istream in range(int(self.script.cfg["NUM_STREAM"])):
            (host, beam_idx, subband) = self.script.cfg["STREAM_" + str(istream)].split(":")
            beam = self.script.cfg["BEAM_" + beam_idx]
            if b == beam:
              port = int(self.script.cfg["STREAM_RECV_PORT"]) + istream
              self.script.log (3, "data_product_configure: connecting to " + host + ":" + str(port))
              sock = sockets.openSocket (DL, host, port, 1)
              if sock:

                req =  "<?req version='1.0' encoding='ISO-8859-1'?>"
                req += "<recv_cmd>"
                req +=   "<command>deconfigure</command>"
                req += "</recv_cmd>"

                sock.send(req)
                recv_reply = sock.recv (65536)
                sock.close()

          # remove the data product
          self.script.log (1, "data_product_configure: self.configured_beams=" + str(self.configured_beams))
          self.configured_beams -= int(self._data_products[data_product_id]['n_beams'])
          self.script.log (1, "data_product_configure: self.configured_beams=" + str(self.configured_beams))
          self._data_products.pop(data_product_id, None)
          self.script.log (1, "data_product_configure: keys=" + str(self._data_products))

        return ("ok", "data product " + str (data_product_id) + " deconfigured")

      if len(msg.arguments) == 6:
        # if the configuration for the specified data product matches extactly the 
        # previous specification for that data product, then no action is required

        if data_product_id in self._data_products and \
            self._data_products[data_product_id]['antennas'] == msg.arguments[1] and \
            self._data_products[data_product_id]['n_channels'] == msg.arguments[2] and \
            self._data_products[data_product_id]['dump_rate'] == msg.arguments[3] and \
            self._data_products[data_product_id]['n_beams'] == msg.arguments[4] and \
            self._data_products[data_product_id]['cbf_source'] == msg.arguments[5]:
          return ("ok", "data_product configuration for " + str(data_product_id) + " matched previous")

        # the data product requires configuration
        else:
          self._data_products[data_product_id] = {}

          self.script.log (1, "data_product_configure: configuring " + data_product_id)

          antennas = msg.arguments[1]
          n_channels = msg.arguments[2]
          dump_rate = msg.arguments[3]
          n_beams = int(msg.arguments[4])
          cbf_source = msg.arguments[5]

          # check if the number of existing + new beams > available
          if self.configured_beams + n_beams > self.available_beams:
            self._data_products.pop(data_product_id, None)
            return ("fail", "requested more beams than were available")

          (cfreq, bwd, nchan) = self.script.cfg["SUBBAND_CONFIG_0"].split(":")
          if nchan != n_channels:
            self._data_products.pop(data_product_id, None)
            return ("fail", "PTUSE configured for " + nchan + " channels")

          # assign the ibeam to the list of beams for this data_product
          self._data_products[data_product_id]['beams'] = []
          for ibeam in range(self.configured_beams, self.configured_beams + n_beams):
            self._data_products[data_product_id]['beams'].append(ibeam)
          self.configured_beams += n_beams

          self._data_products[data_product_id]['antennas'] = antennas
          self._data_products[data_product_id]['n_channels'] = n_channels
          self._data_products[data_product_id]['dump_rate'] = dump_rate
          self._data_products[data_product_id]['n_beams'] = str(n_beams)
          self._data_products[data_product_id]['cbf_source'] = cbf_source

          if n_beams == 1:
            sources = [cbf_source]
          else:
            sources = cbf_source.split(",")

          for i in range(len(sources)):
            (addr, port) = sources[i].split(":")
            (mcast, count) = addr.split("+")

            self.script.log (2, "data_product_configure: parsed " + mcast + "+" + count + ":" + port)
            if not count == "1":
              return ("fail", "CBF source did not match ip_address+1:port")

            mcasts = ["",""]
            ports = [0, 0]

            quartets = mcast.split(".")
            mcasts[0] = ".".join(quartets)
            quartets[3] = str(int(quartets[3])+1)
            mcasts[1] = ".".join(quartets)

            ports[0] = int(port)
            ports[1] = int(port)

            ibeam = self._data_products[data_product_id]['beams'][i]
            b = self.script.beams[ibeam]

            self.script.log (1, "data_product_configure: connecting to RECV instance to update configuration")

            for istream in range(int(self.script.cfg["NUM_STREAM"])):
              (host, beam_idx, subband) = self.script.cfg["STREAM_" + str(istream)].split(":")
              beam = self.script.cfg["BEAM_" + beam_idx]
              if b == beam:
                port = int(self.script.cfg["STREAM_RECV_PORT"]) + istream
                self.script.log (3, "data_product_configure: connecting to " + host + ":" + str(port))
                sock = sockets.openSocket (DL, host, port, 1)
                if sock:

                  req =  "<?req version='1.0' encoding='ISO-8859-1'?>"
                  req += "<recv_cmd>"
                  req +=   "<command>configure</command>"
                  req +=   "<params>"
                  req +=     "<param key='DATA_MCAST_0'>" + mcasts[0] + "</param>"
                  req +=     "<param key='DATA_MCAST_1'>" + mcasts[1] + "</param>"
                  req +=     "<param key='DATA_PORT_0'>" + str(ports[0]) + "</param>"
                  req +=     "<param key='DATA_PORT_1'>" + str(ports[1]) + "</param>"
                  req +=     "<param key='META_MCAST_0'>" + mcasts[0] + "</param>"
                  req +=     "<param key='META_MCAST_1'>" + mcasts[1] + "</param>"
                  req +=     "<param key='META_PORT_0'>" + str(ports[0]) + "</param>"
                  req +=     "<param key='META_PORT_1'>" + str(ports[1]) + "</param>"
                  req +=   "</params>"
                  req += "</recv_cmd>"

                  self.script.log (1, "data_product_configure: sending XML req")
                  sock.send(req)
                  recv_reply = sock.recv (65536)
                  self.script.log (1, "data_product_configure: received " + recv_reply)
                  sock.close()

          return ("ok", "data product " + str (data_product_id) + " configured")

      else:
        return ("fail", "expected 0, 1, 5 or 6 arguments") 

    @request(Str(), Str(), Int())
    @return_reply(Str())
    def request_output_channels (self, req, data_product_id, beam_id, nchannels):
      """Set the number of output channels for the specified beam."""
      (result, message) = self.test_beam_valid (data_product_id, beam_id)
      if result == "fail":
        return (result, message)
      if not self.test_power_of_two (nchannels):
        return ("fail", "number of channels not a power of two")
      if nchannels < 64 or nchannels > 4096:
        return ("fail", "number of channels not within range 64 - 2048")
      self.script.beam_configs[beam_id]["OUTNCHAN"] = str(nchannels)
      return ("ok", "")

    @request(Str(), Str(), Int())
    @return_reply(Str())
    def request_output_bins(self, req, data_product_id, beam_id, nbin):
      """Set the number of output phase bins for the specified beam."""
      (result, message) = self.test_beam_valid (data_product_id, beam_id)
      if result == "fail":
        return (result, message)
      if not self.test_power_of_two(nbin):
        return ("fail", "nbin not a power of two")
      if nbin < 64 or nbin > 2048:
        return ("fail", "nbin not within range 64 - 2048")
      self.script.beam_configs[beam_id]["OUTNBIN"] = str(nbin)
      return ("ok", "")

    @request(Str(), Str(), Int())
    @return_reply(Str())
    def request_output_tsubint (self, req, data_product_id, beam_id, tsubint):
      """Set the length of output sub-integrations the specified beam."""
      (result, message) = self.test_beam_valid (data_product_id, beam_id)
      if result == "fail":
        return (result, message)
      if tsubint < 10 or tsubint > 60:
        return ("fail", "length of output subints must be between 10 and 600 seconds")
      self.script.beam_configs[beam_id]["OUTTSUBINT"] = str(tsubint)
      return ("ok", "")

    @request(Str(), Str(), Float())
    @return_reply(Str())
    def request_output_bins(self, req, data_product_id, beam_id, dm):
      """Set the value of deispersion measure to be removed from the specified beam."""
      (result, message) = self.test_beam_valid (data_product_id, beam_id)
      if result == "fail":
        return (result, message)
      if dm < 0 or dm > 2000:
        return ("fail", "dm not within range 0 - 2000")
      self.script.beam_configs[beam_id]["DM"] = str(dm)
      return ("ok", "")

    # test if a number is a power of two
    def test_power_of_two (self, num):
      return num > 0 and not (num & (num - 1))

    # test whether the specificed data-product-id and beam-id are currently valid
    def test_beam_valid (self, data_product_id, beam_id):

      self.script.log (2, "test_beam_valid: data_product_id="+data_product_id+" configured=" + str(self._data_products.keys()))
      if not data_product_id in self._data_products.keys():
        return ("fail", "data product " + str (data_product_id) + " was not configured")

      self.script.log (2, "test_beam_valid: beam_id="+beam_id+" configured=" + str(self._data_products[data_product_id]["beams"]))
      if len(self._data_products[data_product_id]["beams"]) <= 0:
        return ("fail", "data product " + data_product_id + " had no configured beams")

      for ibeam in self._data_products[data_product_id]['beams']:
        if ibeam < len(self.script.beams) and self.script.beams[ibeam] == beam_id:
          return ("ok", "")
      return ("fail", "data product " + data_product_id + " did not contain beam " + beam_id)

    
    # test whether the specified target exists in the pulsar catalog
    def test_pulsar_valid (self, target):

      self.script.log (2, "test_pulsar_valid: get_psrcat_param (" + target + ", jname)")
      (reply, message) = self.get_psrcat_param (target, "jname")
      if reply != "ok":
        return (reply, message)

      self.script.log (2, "test_pulsar_valid: get_psrcat_param () reply=" + reply + " message=" + message)
      if message == target:
        return ("ok", "")
      else:
        return ("fail", "pulsar " + target + " did not exist in catalog")

    def get_psrcat_param (self, target, param):
      cmd = "psrcat -all " + target + " -c " + param + " -nohead -o short"
      rval, lines = self.script.system (cmd, 3)
      if rval != 0 or len(lines) <= 0:
        return ("fail", "could not use psrcat")

      if lines[0].startswith("WARNING"):
        return ("fail", "pulsar " + target_name + " did not exist in catalog")

      parts = lines[0].split()
      if len(parts) == 2 and parts[0] == "1":
        return ("ok", parts[1])
        
       
###############################################################################
#
if __name__ == "__main__":

  if len(sys.argv) != 2:
    print "ERROR: at most 1 command line argument expected"
    sys.exit(1)

  # this should come from command line argument
  beam_id = sys.argv[1]

  # if the beam_id is < 0, then there is a single KATCP 
  # for all beams, otherwise, 1 per beam

  if int(beam_id) == -1:
    script = KATCPServerDaemon ("spip_katcp")
    beam_id = 0
  else:
    script = KATCPBeamDaemon ("spip_katcp", int(beam_id))

  state = script.configure (DAEMONIZE, DL, "katcp", "katcp")
  if state != 0:
    sys.exit(state)

  server = []

  try:
    server_host = sockets.getHostNameShort()
    server_port = 5000 + int(beam_id)

    script.log(2, "__main__: KATCPServer(" +server_host+"," + str(server_port) + ")")
    server = KATCPServer (server_host, server_port, script)

    script.log(2, "__main__: script.set_katcp()")
    script.set_katcp(server)

    script.log(1, "STARTING SCRIPT")
   
    script.log(2, "__main__: server.start()")
    server.start()

    pubsub_thread = PubSubThread (script, beam_id)
    #pubsub_thread = PubSubSimThread (script, beam_id)
    pubsub_thread.start()

    script.log(2, "__main__: script.main()")
    script.main (beam_id)
    script.log(2, "__main__: script.main() returned")

    script.log(2, "__main__: stopping server")
    if server.running():
      server.stop()
    server.join()

    script.log(2, "__main__: stopping pubsub_thread")
    pubsub_thread.join()

  except:

    script.quit_event.set()

    script.log(-2, "exception caught: " + str(sys.exc_info()[0]))
    print '-'*60
    traceback.print_exc(file=sys.stdout)
    print '-'*60
    if server.running():
      server.stop()
    server.join()

  script.log(1, "STOPPING SCRIPT")
  script.conclude()
  sys.exit(0)
