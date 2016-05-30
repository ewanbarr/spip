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


import os, threading, sys, socket, select, signal, traceback, xmltodict
import errno, time, random, re

from xmltodict import parse
from xml.parsers.expat import ExpatError

from spip.daemons.bases import ServerBased,BeamBased
from spip.daemons.daemon import Daemon
from spip.log_socket import LogSocket
from spip.utils import sockets,times
from spip.threads.reporting_thread import ReportingThread
from spip.threads.socketed_thread import SocketedThread

DAEMONIZE = True
DL = 1

###############################################################
# PubSub daemon
class PubSubThread(SocketedThread):

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
    self.script.log (2, "PubSubThread: message="+str(message))

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
        self.script.log (2, "PubSubThread: closing connection")
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

    while not self.quit_event.isSet():

      # the primary function of the KATCPDaemon is to update the 
      # sensors in the DeviceServer periodically

      time.sleep(2)

      # TODO compute overall device status
      self.katcp._device_status.set_value("ok")

      # connect to SPIP_LMC to retreive temperature information
      for lmc in self.lmcs:

        (host, port) = lmc.split(":")
        self.log(2, "KATCPDaemon::main openSocket("+host+","+port+")")
        sock = sockets.openSocket (DL, host, int(port), 1)
        if sock:
          sock.send(self.lmc_cmd)
          lmc_reply = sock.recv (65536)
          xml = xmltodict.parse(lmc_reply)
          sock.close()

          self.log(2, "KATCPDaemon::main update_lmc_sensors("+host+",[xml])")
          self.update_lmc_sensors(host, xml)

      # connect to SPIP_REPACK to retrieve Pulsar SNR performance
      for repack in self.repacks:
        (host, port) = repack.split(":")
        sock = sockets.openSocket (DL, host, int(port), 1)
        if sock:
          sock.send (self.repack_cmd)
          repack_reply = sock.recv (65536)
          xml = xmltodict.parse(repack_reply)
          sock.close()

          self.log(2, "KATCPDaemon::main update_repack_sensors("+host+",[xml])")
          self.update_repack_sensors(host, xml)

      self.log(2, "KATCPDaemon::main sleep(5)")
      time.sleep(5)

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
    xml +=     "<ra units='hh:mm:ss'>" + self.beam_configs[b]["RA"] + "</ra>"
    xml +=     "<dec units='hh:mm:ss'>" + self.beam_configs[b]["DEC"] + "</dec>"
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
      self.beam_configs[b]["SOURCE"] = ""
      self.beam_configs[b]["RA"] = ""
      self.beam_configs[b]["DEC"] = ""
      self.beam_configs[b]["PID"] = ""
      self.beam_configs[b]["OBSERVER"] = ""
      #self.beam_configs[b]["UTC_START"] = ""
      self.beam_configs[b]["TOBS"] = ""
      self.beam_configs[b]["MODE"] = ""
      self.beam_configs[b]["ADC_SYNC_TIME"] = ""
      self.beam_configs[b]["PERFORM_FOLD"] = "0"
      self.beam_configs[b]["PERFORM_SEARCH"] = "0"
      self.beam_configs[b]["PERFORM_TRANS"] = "0"

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
    self.beam_configs[b]["SOURCE"] = ""
    self.beam_configs[b]["RA"] = ""
    self.beam_configs[b]["DEC"] = ""
    self.beam_configs[b]["PID"] = ""
    self.beam_configs[b]["OBSERVER"] = ""
    #self.beam_configs[b]["UTC_START"] = ""
    self.beam_configs[b]["TOBS"] = ""
    self.beam_configs[b]["MODE"] = ""
    self.beam_configs[b]["ADC_SYNC_TIME"] = "0"
    self.beam_configs[b]["PERFORM_FOLD"] = "0"
    self.beam_configs[b]["PERFORM_SEARCH"] = "0"
    self.beam_configs[b]["PERFORM_TRANS"] = "0"

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

    @request(Int())
    @return_reply(Str())
    def request_capture_init(self, req, data_product_id):
      """Prepare the ingest process for data capture."""
      self.script.log (2, "request_capture_init()")
      if data_product_id in self._data_products:
        # TODO - assume data_product_id is beam name
        b = str(data_product_id)
        host = self.script.tcs_hosts[b]
        port = self.script.tcs_ports[b]
        self.script.log (2, "request_capture_init: opening socket to " + host + ":" + str(port))
        sock = sockets.openSocket (DL, host, int(port), 1)
        if sock:
          xml = self.script.get_xml_config_for_beams (b)
          sock.send(xml + "\r\n")
          reply = sock.recv (65536);
          sock.close()
        return ("ok", "")
      else:
        return ("fail", "data product " + str (data_product_id) + " was not configured")

    @request(Int())
    @return_reply(Str())
    def request_capture_start(self, req, data_product_id):
      """Start capture of SPEAD stream for the data_product_id."""
      if data_product_id in self._data_products:
        # TODO assume data_product_id is beam name
        b = str(data_product_id)
        host = self.script.tcs_hosts[b]
        port = self.script.tcs_ports[b]
        sock = sockets.openSocket (DL, host, int(port), 1)
        if sock:
          xml = self.script.get_xml_start_cmd (b)
          sock.send(xml + "\r\n")
          reply = sock.recv (65536);
          sock.close()

        return ("ok", "")
      else:
        return ("fail", "data product " + str (data_product_id) + " was not configured")

    @request(Int())
    @return_reply(Str())
    def request_capture_stop(self, req, data_product_id):
      """Stop capture of SPEAD stream for the data_product_id."""
      if data_product_id in self._data_products:
        # TODO assume data_product_id is beam name
        b = str(data_product_id) 
        host = self.script.tcs_hosts[b]
        port = self.script.tcs_ports[b]
        sock = sockets.openSocket (DL, host, int(port), 1)
        if sock:
          xml = self.script.get_xml_stop_cmd (b)
          sock.send(xml + "\r\n")
          reply = sock.recv (65536);
          sock.close()

        return ("ok", "")
      else:
        return ("fail", "data product " + str (data_product_id) + " was not configured")

    @request(Int())
    @return_reply(Str())
    def request_capture_done(self, req, data_product_id):
      """Terminte the ingest process for the specified data_product_id."""
      if data_product_id in self._data_products:
        # TODO instruct SPIP backend for beam to conclude observation 
        return ("ok", "")
      else:
        return ("fail", "data product " + str (data_product_id) + " was not configured")

    @return_reply(Str())
    def request_data_product_configure(self, req, msg):
      """Prepare and configure for the reception of the data_product_id."""
      self.script.log (2, "request_data_product_configure ()")
      if len(msg.arguments) == 0:
        return ("ok", "configured data products: TBD")

      data_product_id = int(msg.arguments[0])

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

      if msg.arguments[1] == "":
        return ("ok", "deconfigured data_product " + str (data_product_id))

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
          self._data_products[data_product_id]['antennas'] = msg.arguments[1]
          self._data_products[data_product_id]['n_channels'] = msg.arguments[2]
          self._data_products[data_product_id]['dump_rate'] = msg.arguments[3]
          self._data_products[data_product_id]['n_beams'] = msg.arguments[4]
          self._data_products[data_product_id]['cbf_source'] = msg.arguments[5]
          return ("ok", "data product " + str (data_product_id) + " configured")

      else:
        return ("fail", "expected 0, 1 or 6 arguments") 
       
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
    pubsub_thread.start()

    script.log(2, "__main__: script.main()")
    script.main (beam_id)
    script.log(1, "__main__: script.main() returned")

    if server.running():
      server.stop()
    server.join()

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
