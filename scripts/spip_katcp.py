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

from spip import config
from spip.daemons.bases import ServerBased,BeamBased
from spip.daemons.daemon import Daemon
from spip.log_socket import LogSocket
from spip.utils import sockets,times
from spip.threads.reporting_thread import ReportingThread

DAEMONIZE = False
DL = 2

###############################################################
# KATCP daemon
class KATCPDaemon(Daemon):

  def __init__ (self, name, id):
    Daemon.__init__(self, name, str(id))
    self.beam_states = {}
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
    self.katcp._beam_list_result.set_value (r)

    r = ""
    if len(self.hosts) > 1:
      r = ",".join(str(b) for b in self.beams)
    elif len(self.hosts) == 1:
      r = str(self.hosts[0])
    else:
      self.log(-2, "set_katcp: no hosts configured")
    self.katcp._host_list_result.set_value (r)

  #############################################################################
  # script core
  def main (self, id):

    # connect to the various scripts running to collect the information
    # to be provided by the KATCPServer instance as sensors

    while not self.quit_event.isSet():

      # the primary function of the KATCPDaemon is to update the 
      # sensors in the DeviceServer periodically

      time.sleep(2)

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

  def update_lmc_sensors (self, host, xml):

    self.katcp._host_sensors[host]["disk_size"].set_value (float(xml["lmc_reply"]["disk"]["size"]["#text"]))
    self.katcp._host_sensors[host]["disk_available"].set_value (float(xml["lmc_reply"]["disk"]["available"]["#text"]))

    self.katcp._host_sensors[host]["num_cores"].set_value (int(xml["lmc_reply"]["system_load"]["@ncore"]))
    self.katcp._host_sensors[host]["load1"].set_value (float(xml["lmc_reply"]["system_load"]["load1"]))
    self.katcp._host_sensors[host]["load5"].set_value (float(xml["lmc_reply"]["system_load"]["load5"]))
    self.katcp._host_sensors[host]["load15"].set_value (float(xml["lmc_reply"]["system_load"]["load15"]))

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
      self.beam_states[i+1] = {}
      self.beam_states[i+1]["NAME"] = self.cfg["BEAM_"+str(i)]
      self.beam_states[i+1]["SNR"] = 0
      self.beam_states[i+1]["POWER"] = 0
    self.beams = self.beam_states.keys()

class KATCPBeamDaemon (KATCPDaemon, BeamBased):

  def __init__ (self, name, id):
    KATCPDaemon.__init__(self, name, str(id))
    BeamBased.__init__(self, str(id), self.cfg)

    self.beam_states[i+1] = {}
    self.beam_states[i+1]["NAME"] = self.cfg["BEAM_"+str(i)]
    self.beam_states[i+1]["SNR"] = 0
    self.beam_states[i+1]["POWER"] = 0
    self.beams = self.beam_states.keys()


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
      DeviceServer.__init__(self, server_host, server_port)

    def setup_sensors(self):
      """Setup server sensors."""
      self.script.log(2, "KATCPServer::setup_sensors()")

      self._beam_list_result = Sensor.string("beam_list.result",
        description="list of configured beams",
        unit="",
        default="")
      self.add_sensor(self._beam_list_result)

      # setup host based sensors   
      self._host_list_result  = Sensor.string("host_list.result",
        description="list of configured servers",
        unit="",
        default="")
      self.add_sensor(self._host_list_result)

      self.script.log(2, "KATCPServer::setup_sensors lmcs="+str(self.script.lmcs))
      for lmc in self.script.lmcs:
        (host, port) = lmc.split(":")
        self.setup_sensors_host (host, port)

      self.script.log(2, "KATCPServer::setup_sensors beams="+str(self.script.beams))
      for b in self.script.beams:
        self.setup_sensors_beam (b)

    # add sensors based on the reply from the specified host
    def setup_sensors_host (self, host, port):

      self.script.log(2, "KATCPServer::setup_sensors_hosts("+host+","+port+")")
      sock = sockets.openSocket (DL, host, int(port), 1)
      if sock:
        sock.send (self.script.lmc_cmd + "\r\n")
        lmc_reply = sock.recv (65536)
        sock.close()
        xml = xmltodict.parse(lmc_reply)

        self._host_sensors[host] = {}
        self._host_sensors[host]["sensors"] = {}

        # Disk sensors
        disk_prefix = host+".disk"
        self._host_sensors[host]["disk_size"] = Sensor.float(disk_prefix+".size",
          description=host+": disk size",
          unit="MB",
          params[8192,1e9],
          default=0)
        self._host_sensors[host]["disk_available"] = Sensor.float(disk_prefix+".available",
          description=host+": disk available space",
          unit="MB",
          params[1024,1e9],
          default=0)
        self.add_sensor(self._host_sensors[host]["disk_size"])
        self.add_sensor(self._host_sensors[host]["disk_available"])

        # Server Load sensors
        self._host_sensors[host]["num_cores"] = Sensor.integer (host+".num_cores",
          description=host+": disk available space",
          unit="MB",
          params[1,64],
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
      self._beam_list_result.set_value(r)
      return (state, r)

    @request()
    @return_reply(Str())
    def request_host_list(self, req):
      """Return the list of configured servers."""
      r = self._host_list_result.value()
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

    def request_raw_reverse(self, req, msg):
      """
      A raw request handler to demonstrate the calling convention if
      @request decoraters are not used. Reverses the message arguments.
      """
      # msg is a katcp.Message.request object
      reversed_args = msg.arguments[::-1]
      # req.make_reply() makes a katcp.Message.reply using the correct request
      # name and message ID
      return req.make_reply(*reversed_args)


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
    script = KATCPServerDaemon ("katcp")
    beam_id = 0
  else:
    script = KATCPBeamDaemon ("katcp", beam_id)

  state = script.configure (DAEMONIZE, DL, "katcp", "katcp")
  if state != 0:
    sys.exit(state)

  server = []

  try:
    server_host = sockets.getHostNameShort()
    server_port = 5000 + int(beam_id)
    server = KATCPServer (server_host, server_port, script)

    script.set_katcp(server)

    script.log(1, "STARTING SCRIPT")
   
    server.start()

    script.main (beam_id)

    if server.running():
      server.stop()
    server.join()

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
