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

import os, threading, sys, socket, select, signal, traceback, xmltodict, re
import errno

from xmltodict import parse
from xml.parsers.expat import ExpatError

from spip.utils import sockets
from spip.utils import catalog

DAEMONIZE = True
DL = 1

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
    self._data_product = {}
    self._data_product["id"] = "None"

    self.data_product_res = []
    self.data_product_res.append(re.compile ("^[a-zA-Z]+_1"))
    self.data_product_res.append(re.compile ("^[a-zA-Z]+_2"))
    self.data_product_res.append(re.compile ("^[a-zA-Z]+_3"))
    self.data_product_res.append(re.compile ("^[a-zA-Z]+_4"))

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

    self._beam_name = Sensor.string("beam-name",
      description="name of configured beam",
      unit="",
      default="")
    self.add_sensor(self._beam_name)

    # setup host based sensors   
    self._host_name = Sensor.string("host-name",
      description="hostname of this server",
      unit="",
      default="")
    self.add_sensor(self._host_name)

    self.script.log(2, "KATCPServer::setup_sensors lmc="+str(self.script.lmc))
    (host, port) = self.script.lmc.split(":")
    self.setup_sensors_host (host, port)

    self.script.log(2, "KATCPServer::setup_sensors beams="+str(self.script.beam))
    self.setup_sensors_beam (self.script.beam_name)

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

      self._host_sensors = {}

      # Disk sensors
      self.script.log(2, "KATCPServer::setup_sensors_host configuring disk sensors")
      disk_prefix = host+".disk"
      self._host_sensors["disk_size"] = Sensor.float(disk_prefix+".size",
        description=host+": disk size",
        unit="MB",
        params=[8192,1e9],
        default=0)
      self._host_sensors["disk_available"] = Sensor.float(disk_prefix+".available",
        description=host+": disk available space",
        unit="MB",
        params=[1024,1e9],
        default=0)
      self.add_sensor(self._host_sensors["disk_size"])
      self.add_sensor(self._host_sensors["disk_available"])

      # Server Load sensors
      self.script.log(2, "KATCPServer::setup_sensors_host configuring load sensors")
      self._host_sensors["num_cores"] = Sensor.integer (host+".num_cores",
        description=host+": disk available space",
        unit="MB",
        params=[1,64],
        default=0)

      self._host_sensors["load1"] = Sensor.float(host+".load.1min",
        description=host+": 1 minute load ",
        unit="",
        default=0)

      self._host_sensors["load5"] = Sensor.float(host+".load.5min",
        description=host+": 5 minute load ",
        unit="",
        default=0)
      
      self._host_sensors["load15"] = Sensor.float(host+".load.15min",
        description=host+": 15 minute load ",
        unit="",
        default=0)

      self.add_sensor(self._host_sensors["num_cores"])
      self.add_sensor(self._host_sensors["load1"])
      self.add_sensor(self._host_sensors["load5"])
      self.add_sensor(self._host_sensors["load15"])

      cpu_temp_pattern  = re.compile("cpu[0-9]+_temp")
      fan_speed_pattern = re.compile("fan[0-9,a-z]+")
      power_supply_pattern = re.compile("ps[0-9]+_status")
        
      self.script.log(2, "KATCPServer::setup_sensors_host configuring other metrics")

      if not xml["lmc_reply"]["sensors"] == None:

        for sensor in xml["lmc_reply"]["sensors"]["metric"]:
          name = sensor["@name"]
          if name == "system_temp":
            self._host_sensors[name] = Sensor.float((host+".system_temp"),
              description=host+": system temperature",
              unit="C",
              params=[-20,150],
              default=0)
            self.add_sensor(self._host_sensors[name])

          if cpu_temp_pattern.match(name):
            (cpu, junk) = name.split("_")
            self._host_sensors[name] = Sensor.float((host+"." + name),
              description=host+": "+ cpu +" temperature",
              unit="C",
              params=[-20,150],
              default=0)
            self.add_sensor(self._host_sensors[name])

          if fan_speed_pattern.match(name):
            self._host_sensors[name] = Sensor.float((host+"." + name),
              description=host+": "+name+" speed",
              unit="RPM",
              params=[0,20000],
              default=0)
            self.add_sensor(self._host_sensors[name])

          if power_supply_pattern.match(name):
            self._host_sensors[name] = Sensor.boolean((host+"." + name),
              description=host+": "+name,
              unit="",
              default=0)
            self.add_sensor(self._host_sensors[name])

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
    self._beam_sensors = {}

    self.script.log(2, "KATCPServer::setup_sensors_beam ="+b)

    self._beam_sensors["observing"] = Sensor.boolean("observing",
      description="Beam " + b + " is observing",
      unit="",
      default=0)
    self.add_sensor(self._beam_sensors["observing"])

    self._beam_sensors["snr"] = Sensor.float("snr",
      description="SNR of Beam "+b,
      unit="",
      params=[0,1e9],
      default=0)
    self.add_sensor(self._beam_sensors["snr"])

    self._beam_sensors["power"] = Sensor.float("power",
      description="Power Level of Beam "+b,
      unit="",
      default=0)
    self.add_sensor(self._beam_sensors["power"])

    self._beam_sensors["integrated"] = Sensor.float("integrated",
      description="Length of integration for Beam "+b,
      unit="",
      default=0)
    self.add_sensor(self._beam_sensors["integrated"])

  @request()
  @return_reply(Str())
  def request_beam(self, req):
    """Return the configure beam name."""
    return ("ok", self._beam_name.value())

  @request()
  @return_reply(Str())
  def request_host_name(self, req):
    """Return the name of this server."""
    return ("ok", self._host_name.value())

  @request()
  @return_reply(Float())
  def request_snr(self, req):
    """Return the SNR for this beam."""
    return ("ok", self._beam_sensors["snr"].value())

  @request()
  @return_reply(Float())
  def request_power(self, req):
    """Return the standard deviation of the 8-bit power level."""
    return ("ok", self._beam_sensors["power"].value())

  @request(Str(), Float())
  @return_reply(Str())
  def request_sync_time (self, req, data_product_id, adc_sync_time):
    """Set the ADC_SYNC_TIME for beam of the specified data product."""
    if not data_product_id == self._data_product["id"]:
      return ("fail", "data product " + str (data_product_id) + " was not configured")
    self.script.beam_config["lock"].acquire()
    self.script.beam_config["ADC_SYNC_TIME"] = str(adc_sync_time)
    self.script.beam_config["lock"].release()
    return ("ok", "")

  @request(Str(), Str())
  @return_reply(Str())
  def request_target_start (self, req, data_product_id, target_name):
    """Commence data processing on specific data product and beam using target."""
    self.script.log (1, "request_target_start(" + data_product_id + "," + target_name+")")

    self.script.beam_config["lock"].acquire()
    self.script.beam_config["ADC_SYNC_TIME"] = self.script.cam_config["ADC_SYNC_TIME"]
    self.script.beam_config["OBSERVER"] = self.script.cam_config["OBSERVER"]
    self.script.beam_config["ANTENNAE"] = self.script.cam_config["ANTENNAE"]
    self.script.beam_config["SCHEDULE_BLOCK_ID"] = self.script.cam_config["SCHEDULE_BLOCK_ID"]
    self.script.beam_config["EXPERIMENT_ID"] = self.script.cam_config["EXPERIMENT_ID"]
    self.script.beam_config["DESCRIPTION"] = self.script.cam_config["DESCRIPTION"]
    self.script.beam_config["lock"].release()

    # check the pulsar specified is listed in the catalog
    (result, message) = self.test_pulsar_valid (target_name)
    if result != "ok":
      return (result, message)

    # check the ADC_SYNC_TIME is valid for this beam
    if self.script.beam_config["ADC_SYNC_TIME"] == "0":
      return ("fail", "ADC Synchronisation Time was not valid")
  
    # set the pulsar name, this should include a check if the pulsar is in the catalog
    self.script.beam_config["lock"].acquire()
    if self.script.beam_config["MODE"] == "CAL":
      target_name = target_name + "_R"
    self.script.beam_config["SOURCE"] = target_name
    self.script.beam_config["lock"].release()

    host = self.script.tcs_host
    port = self.script.tcs_port

    self.script.log (2, "request_target_start: opening socket for beam " + beam_id + " to " + host + ":" + str(port))
    sock = sockets.openSocket (DL, host, int(port), 1)
    if sock:
      xml = self.script.get_xml_config()
      sock.send(xml + "\r\n")
      reply = sock.recv (65536)

      xml = self.script.get_xml_start_cmd()
      sock.send(xml + "\r\n")
      reply = sock.recv (65536)

      sock.close()
      return ("ok", "")
    else:
      return ("fail", "could not connect to TCS")

  @request(Str())
  @return_reply(Str())
  def request_target_stop (self, req, data_product_id):
    """Cease data processing with target_name."""
    self.script.log (1, "request_target_stop(" + data_product_id+")")

    self.script.beam_config["lock"].acquire()
    self.script.beam_config["SOURCE"] = ""
    self.script.beam_config["lock"].release()

    host = self.script.tcs_host
    port = self.script.tcs_port
    sock = sockets.openSocket (DL, host, int(port), 1)
    if sock:
      xml = self.script.get_xml_stop_cmd ()
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
    self.script.log (1, "request_capture_init: " + str(data_product_id) )
    if not data_product_id == self._data_product["id"]:
      return ("fail", "data product " + str (data_product_id) + " was not configured")
    return ("ok", "")

  @request(Str())
  @return_reply(Str())
  def request_capture_done(self, req, data_product_id):
    """Terminte the ingest process for the specified data_product_id."""
    self.script.log (1, "request_capture_done: " + str(data_product_id))
    if not data_product_id == self._data_product["id"]:
      return ("fail", "data product " + str (data_product_id) + " was not configured")
    return ("ok", "")

  @return_reply(Str())
  def request_configure(self, req, msg):
    """Prepare and configure for the reception of the data_product_id."""
    self.script.log (1, "request_configure: nargs= " + str(len(msg.arguments)) + " msg=" + str(msg))
    if len(msg.arguments) == 0:
      self.script.log (-1, "request_configure: no arguments provided")
      return ("ok", "configured data products: TBD")

    # the sub-array identifier
    data_product_id = msg.arguments[0]

    if len(msg.arguments) == 1:
      self.script.log (1, "request_configure: request for configuration of " + str(data_product_id))
      if data_product_id == self._data_product["id"]:
        configuration = str(data_product_id) + " " + \
                        str(self._data_product['antennas']) + " " + \
                        str(self._data_product['n_channels']) + " " + \
                        str(self._data_product['cbf_source'])
        self.script.log (1, "request_configure: configuration of " + str(data_product_id) + "=" + configuration)
        return ("ok", configuration)
      else:
        self.script.log (-1, "request_configure: no configuration existed for " + str(data_product_id))
        return ("fail", "no configuration existed for " + str(data_product_id))

    if len(msg.arguments) == 4:
      # if the configuration for the specified data product matches extactly the 
      # previous specification for that data product, then no action is required
      self.script.log (1, "configure: configuring " + str(data_product_id))

      if data_product_id == self._data_product["id"] and \
          self._data_product['antennas'] == msg.arguments[1] and \
          self._data_product['n_channels'] == msg.arguments[2] and \
          self._data_product['cbf_source'] == msg.arguments[3]:
        response = "configuration for " + str(data_product_id) + " matched previous"
        self.script.log (1, "configure: " + response)
        return ("ok", response)

      # the data product requires configuration
      else:
        self.script.log (1, "configure: new data product " + data_product_id)

        # determine which sub-array we are matched against
        the_sub_array = -1
        for i in range(4):
          self.script.log (1, "configure: testing self.data_product_res[" + str(i) +"].match(" + data_product_id +")")
          if self.data_product_res[i].match (data_product_id):
            the_sub_array = i + 1

        if the_sub_array == -1:
          self.script.log (1, "configure: could not match subarray from " + data_product_id)
          return ("fail", "could not data product to sub array")

        self.script.log (1, "configure: restarting pubsub for subarray " + str(the_sub_array))
        self.script.pubsub.set_sub_array (the_sub_array, self.script.beam_name)
        self.script.pubsub.restart()

        antennas = msg.arguments[1]
        n_channels = msg.arguments[2]
        cbf_source = msg.arguments[3]

        # check if the number of existing + new beams > available
        (cfreq, bwd, nchan) = self.script.cfg["SUBBAND_CONFIG_0"].split(":")
        if nchan != n_channels:
          self._data_product.pop(data_product_id, None)
          response = "PTUSE configured for " + nchan + " channels"
          self.script.log (-1, "configure: " + response)
          return ("fail", response)

        self._data_product['id'] = data_product_id
        self._data_product['antennas'] = antennas
        self._data_product['n_channels'] = n_channels
        self._data_product['cbf_source'] = cbf_source

        # parse the CBF_SOURCE to determine multicast groups
        (addr, port) = cbf_source.split(":")
        (mcast, count) = addr.split("+")

        self.script.log (2, "configure: parsed " + mcast + "+" + count + ":" + port)
        if not count == "1":
          response = "CBF source did not match ip_address+1:port"
          self.script.log (-1, "configure: " + response)
          return ("fail", response)

        mcasts = ["",""]
        ports = [0, 0]

        quartets = mcast.split(".")
        mcasts[0] = ".".join(quartets)
        quartets[3] = str(int(quartets[3])+1)
        mcasts[1] = ".".join(quartets)

        ports[0] = int(port)
        ports[1] = int(port)

        self.script.log (1, "configure: connecting to RECV instance to update configuration")

        for istream in range(int(self.script.cfg["NUM_STREAM"])):
          (host, beam_idx, subband) = self.script.cfg["STREAM_" + str(istream)].split(":")
          beam = self.script.cfg["BEAM_" + beam_idx]
          if beam == self.script.beam_name:

            # reset ADC_SYNC_TIME on the beam
            self.script.beam_config["lock"].acquire()
            self.script.beam_config["ADC_SYNC_TIME"] = "0";
            self.script.beam_config["lock"].release()

            port = int(self.script.cfg["STREAM_RECV_PORT"]) + istream
            self.script.log (3, "configure: connecting to " + host + ":" + str(port))
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

              self.script.log (1, "configure: sending XML req")
              sock.send(req)
              recv_reply = sock.recv (65536)
              self.script.log (1, "configure: received " + recv_reply)
              sock.close()

      return ("ok", "data product " + str (data_product_id) + " configured")

    else:
      response = "expected 0, 1 or 4 arguments"
      self.script.log (-1, "configure: " + response)
      return ("fail", response)

  @return_reply(Str())
  def request_deconfigure(self, req, msg):
    """Deconfigure for the data_product."""

    if len(msg.arguments) == 0:
      self.script.log (-1, "request_configure: no arguments provided")
      return ("fail", "expected 1 argument")

    # the sub-array identifier
    data_product_id = msg.arguments[0]

    self.script.log (1, "configure: deconfiguring " + str(data_product_id))

    # check if the data product was previously configured
    if not data_product_id == self._data_product["id"]:
      response = str(data_product_id) + " did not match configured data product [" + self._data_product["id"] + "]"
      self.script.log (-1, "configure: " + response)
      return ("fail", response)

    for istream in range(int(self.script.cfg["NUM_STREAM"])):
      (host, beam_idx, subband) = self.script.cfg["STREAM_" + str(istream)].split(":")
      if self.script.beam_name == self.script.cfg["BEAM_" + beam_idx]:

        # reset ADC_SYNC_TIME on the beam
        self.script.beam_config["lock"].acquire()
        self.script.beam_config["ADC_SYNC_TIME"] = "0";
        self.script.beam_config["lock"].release()

        port = int(self.script.cfg["STREAM_RECV_PORT"]) + istream
        self.script.log (3, "configure: connecting to " + host + ":" + str(port))
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
      self._data_product["id"] = "None"

    response = "data product " + str(data_product_id) + " deconfigured"
    self.script.log (1, "configure: " + response)
    return ("ok", response)

  @request(Int())
  @return_reply(Str())
  def request_output_channels (self, req, nchannels):
    """Set the number of output channels."""
    if not self.test_power_of_two (nchannels):
      return ("fail", "number of channels not a power of two")
    if nchannels < 64 or nchannels > 4096:
      return ("fail", "number of channels not within range 64 - 2048")
    self.script.beam_config["OUTNCHAN"] = str(nchannels)
    return ("ok", "")

  @request(Int())
  @return_reply(Str())
  def request_output_bins(self, req, nbin):
    """Set the number of output phase bins."""
    if not self.test_power_of_two(nbin):
      return ("fail", "nbin not a power of two")
    if nbin < 64 or nbin > 2048:
      return ("fail", "nbin not within range 64 - 2048")
    self.script.beam_config["OUTNBIN"] = str(nbin)
    return ("ok", "")

  @request(Int())
  @return_reply(Str())
  def request_output_tsubint (self, req, tsubint):
    """Set the length of output sub-integrations."""
    if tsubint < 10 or tsubint > 60:
      return ("fail", "length of output subints must be between 10 and 600 seconds")
    self.script.beam_config["OUTTSUBINT"] = str(tsubint)
    return ("ok", "")

  @request(Float())
  @return_reply(Str())
  def request_dm(self, req, dm):
    """Set the value of dispersion measure to be removed"""
    if dm < 0 or dm > 2000:
      return ("fail", "dm not within range 0 - 2000")
    self.script.beam_config["DM"] = str(dm)
    return ("ok", "")

  @request(Float())
  @return_reply(Str())
  def request_cal_freq(self, req, cal_freq):
    """Set the value of noise diode firing frequecny in Hz."""
    if cal_freq < 0 or cal_freq > 1000:
      return ("fail", "CAL freq not within range 0 - 1000")
    self.script.beam_config["CALFREQ"] = str(cal_freq)
    if cal_freq == 0:
      self.script.beam_config["MODE"] = "PSR"
    else:
      self.script.beam_config["MODE"] = "CAL"
    return ("ok", "")

  # test if a number is a power of two
  def test_power_of_two (self, num):
    return num > 0 and not (num & (num - 1))

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

