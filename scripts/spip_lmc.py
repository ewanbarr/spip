#!/usr/bin/env python

##############################################################################
#  
#     Copyright (C) 2015 by Andrew Jameson
#     Licensed under the Academic Free License version 2.1
# 
###############################################################################

#
# spip_lmc - 
#

import os, socket, threading, sys, errno, traceback, select, xmltodict, subprocess
from time import sleep
import spip_lmc_monitor as lmc_mon

from spip.daemons.bases import HostBased
from spip.daemons.daemon import Daemon
from spip.utils.sockets import getHostNameShort
from spip.config import Config
from spip.utils.core import system

DAEMONIZE = True
DL        = 1

#################################################################
# clientThread
#
# launches client daemons for the specified stream
 
class clientThread (threading.Thread):

  def __init__ (self, stream_id, script, states):

    threading.Thread.__init__(self)
    self.stream_id = stream_id
    self.daemon_exit_wait = 10
    self.states = states
    self.parent = script
    self.daemons = {}
    self.ranks = []

    if stream_id >= 0:
      self.daemon_list = script.cfg["CLIENT_DAEMONS"].split()
      self.control_dir = script.cfg["CLIENT_CONTROL_DIR"]
      self.prefix = "clientThread["+str(stream_id)+"] "
    else:
      self.daemon_list = script.cfg["SERVER_DAEMONS"].split()
      self.control_dir = script.cfg["SERVER_CONTROL_DIR"]
      self.prefix = "serverThread "

    ranked_daemons = self.daemon_list
    for ranked_daemon in ranked_daemons:
      daemon, rank = ranked_daemon.split(":")
      if not rank in self.daemons:
        self.daemons[rank] = []
      self.daemons[rank].append(daemon)

    self.ranks = self.daemons.keys()
    self.ranks.sort()
    self.process_suffix = " " + str(self.stream_id)
    self.file_suffix = "_" + str(self.stream_id)

  def run (self):

    self.parent.log(2, self.prefix + "thread running")

    try:

      # start all of the daemons
      self.start_daemons (self.ranks)

      # monitor the daemons in each stream and 
      # process control commands specific to the stream
      while (not self.parent.quit_event.isSet()):

        # check if a reload has been requested
        if self.stream_id >= 0 and self.parent.reload_clients[self.stream_id]:
          self.parent.log(1, self.prefix + "reloading stream")
          self.stop_daemons (self.ranks)
          self.parent.log(1, self.prefix + "daemons stopped")
          self.start_daemons (self.ranks)
          self.parent.log(1, self.prefix + "daemons started")
          self.parent.reload_clients[self.stream_id] = False
          self.parent.log(1, self.prefix + "stream reloaded")

        for rank in self.ranks:
          for daemon in self.daemons[rank]: 
            cmd = "pgrep -f '^python " + self.parent.cfg["SCRIPTS_DIR"] + "/" + daemon + ".py" + self.process_suffix + "' | wc -l"
            rval, lines = self.parent.system (cmd, 2)
            self.states[daemon] = (rval == 0)
        counter = 5
        while (not self.parent.quit_event.isSet() and counter > 0):
          sleep(1)
          counter -= 1

      self.parent.log(1, self.prefix + "asking daemons to quit")
      self.stop_daemons (self.ranks)
      self.parent.log(1, self.prefix + "thread exiting")

    except:
      self.parent.quit_event.set()

      formatted_lines = traceback.format_exc().splitlines()
      self.parent.log(1, self.prefix + '-'*60)
      for line in formatted_lines:
        self.parent.log(1, self.prefix + line)
      self.parent.log(1, self.prefix + '-'*60)

  # start all the daemons in ranks (ascending)
  def start_daemons (self, ranks):

    self.parent.system_lock.acquire()
    to_sleep = 2

    for rank in ranks:

      if rank > 0:
        sleep(to_sleep)
        to_sleep = 0

      self.parent.log(1, self.prefix + "launching daemons of rank " + rank)
      for daemon in self.daemons[rank]:
        self.states[daemon] = False
        cmd = "python " + self.parent.cfg["SCRIPTS_DIR"] + "/" + daemon + ".py" + self.process_suffix
        self.parent.log(1, self.prefix + cmd)
        rval, lines = self.parent.system (cmd)
        if rval:
          for line in lines:
            self.parent.log(-2, self.prefix + line)
          self.parent.quit_event.set()
        else:
          for line in lines:
            self.parent.log(2, self.prefix + line)
      self.parent.log (1, self.prefix + "launched daemons of rank " + rank)

    self.parent.log(1, self.prefix + "launched all daemons")
    self.parent.system_lock.release ()

  # stop the daemons listed in ranks in reverse order
  def stop_daemons (self, ranks):

    for rank in ranks[::-1]:
      if rank == 0:
        self.parent.log (1, self.prefix + " sleep(5) for rank 0 daemons")
        sleep(5)

      for daemon in self.daemons[rank]:
        open(self.control_dir + "/" + daemon + self.file_suffix + ".quit", 'w').close()

      rank_timeout = self.daemon_exit_wait
      daemon_running = 1

      while daemon_running and rank_timeout > 0:
        daemon_running = 0
        self.parent.system_lock.acquire()
        for daemon in self.daemons[rank]:
          cmd = "pgrep -f '^python " + self.parent.cfg["SCRIPTS_DIR"] + "/" + daemon + ".py" + self.process_suffix + "'"
          rval, lines = self.parent.system (cmd, 3)
          if rval == 0 and len(lines) > 0:
            daemon_running = 1
            self.parent.log(2, self.prefix + "daemon " + daemon + " with rank " +
                        str(rank) + " still running")
        self.parent.system_lock.release()
        if daemon_running:
          self.parent.log(3, self.prefix + "daemons " + "with rank " + str(rank) +
                      " still running")
          sleep(1)

      # if any daemons in this rank timed out, hard kill them
      if rank_timeout == 0:
        self.parent.system_lock.acquire()
        for daemon in self.daemons[rank]:
          cmd = "pkill -f ''^python " + self.parent.cfg["SCRIPTS_DIR"] + "/" + daemon + ".py" + self.process_suffix + "'"
          rval, lines = self.parent.system (cmd, 3)
        self.parent.system_lock.release()

      # remove daemon.quit files for this rank
      for daemon in self.daemons[rank]:
        os.remove (self.control_dir + "/" + daemon + self.file_suffix + ".quit")


################################################################$

class LMCDaemon (Daemon,HostBased):

  def __init__ (self, name, hostname):
    Daemon.__init__(self, name, hostname)
    HostBased.__init__(self, hostname, self.cfg)

  def main (self):

    control_thread = []
    self.client_threads = {}
    self.reload_clients = {}
    self.server_thread = []
    self.system_lock = threading.Lock()

    # find matching client streams for this host
    client_streams = []
    for istream in range(int(self.cfg["NUM_STREAM"])):
      (req_host, beam_id, subband_id) = Config.getStreamConfig (istream, self.cfg)
      if req_host == self.req_host:
        client_streams.append(istream)

    # find matching server stream
    server_streams = []
    if self.cfg["SERVER_HOST"] == self.req_host:
      server_streams.append(-1)

    daemon_states = {}

    for stream in server_streams:
      self.log(2, "main: client_thread[-1] = clientThread(-1)")
      daemon_states[-1] = {}
      server_thread = clientThread(-1, self, daemon_states[-1])
      self.log(2, "main: client_thread[-1].start()")
      server_thread.start()
      self.log(2, "main: client_thread[-1] started")

    sleep(1)

    # start a control thread for each stream
    for stream in client_streams:
      daemon_states[stream] = {}
      self.log(2, "main: client_thread["+str(stream)+"] = clientThread ("+str(stream)+")")
      self.reload_clients[stream] = False
      self.client_threads[stream] = clientThread(stream, self, daemon_states[stream])
      self.log(2, "main: client_thread["+str(stream)+"].start()")
      self.client_threads[stream].start()
      self.log(2, "main: client_thread["+str(stream)+"] started!")


    # main thread
    disks_to_monitor = [self.cfg["CLIENT_DIR"]]

    # create socket for LMC commands
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind((self.req_host, int(self.cfg["LMC_PORT"])))
    sock.listen(5)

    can_read = [sock]
    can_write = []
    can_error = []
    timeout = 1
    hw_poll = 5
    counter = 0 

    sensors = {}

    # monitor / control loop
    while not self.quit_event.isSet():

      self.log(3, "Main Loop: counter="+str(counter))

      while (counter == 0):

        self.log(2, "Refreshing monitoring points")

        self.log(3, "main: getDiskCapacity ()")
        rval, disks = lmc_mon.getDiskCapacity (disks_to_monitor, DL)
        self.log(3, "main: " + str(disks))

        self.log(3, "main: getLoads()")
        rval, loads = lmc_mon.getLoads (DL)
        self.log(3, "main: " + str(loads))

        self.log(3, "main: getSMRBCapacity(" + str(client_streams)+ ")")
        rval, smrbs = lmc_mon.getSMRBCapacity (client_streams, self.quit_event, DL)
        self.log(3, "main: " + str(smrbs))

        self.log(3, "main: getIPMISensors()")
        rval, sensors = lmc_mon.getIPMISensors (DL)
        self.log(3, "main: " + str(sensors))

        counter = hw_poll

      self.log(3, "main: calling select len(can_read)="+str(len(can_read)))
      timeout = 1
      did_read = []
      did_write = []
      did_error = []

      try:
        did_read, did_write, did_error = select.select(can_read, can_write, can_error, timeout)
      except select.error as e:
        self.quit_event.set()
      else:
        self.log(3, "main: read="+str(len(did_read))+" write="+
                    str(len(did_write))+" error="+str(len(did_error)))

      if (len(did_read) > 0):
        for handle in did_read:
          if (handle == sock):
            (new_conn, addr) = sock.accept()
            self.log(2, "main: accept connection from "+repr(addr))
            # add the accepted connection to can_read
            can_read.append(new_conn)
            # new_conn.send("Welcome to the LMC interface\r\n")

          # an accepted connection must have generated some data
          else:
            try:
              raw = handle.recv(4096)
            except socket.error, e:
              if e.errno == errno.ECONNRESET:
                self.log(2, "main: closing connection")
                handle.close()
                for i, x in enumerate(can_read):
                  if (x == handle):
                    del can_read[i]
              else:
                raise e
            else: 
              message = raw.strip()
              self.log(2, "main: message='" + message+"'")

              if len(message) == 0:
                self.log(2, "main: closing connection")
                handle.close()
                for i, x in enumerate(can_read):
                  if (x == handle):
                    del can_read[i]

              else:
                xml = xmltodict.parse(message)

                command = xml["lmc_cmd"]["command"]

                if command == "reload_clients":
                  self.log(1, "Reloading clients")
                  for stream in client_streams:
                    self.reload_clients[stream] = True
            
                  all_reloaded = False
                  while (not self.parent.quit_event.isSet() and not all_reloaded):
                    all_reloaded = True
                    for stream in client_streams:
                      if not self.reload_clients[stream]:
                        all_reloaded = False
                    if not all_reloaded:
                      self.log(1, "Waiting for clients to reload")
                      sleep(1)

                  self.log(1, "Clients reloaded")
                  response = "<lmc_reply>OK</lmc_reply>"

                if command == "daemon_status":
                  response = ""
                  response += "<lmc_reply>"

                  for stream in server_streams:
                    response += "<stream id='" + str(stream) +"'>"
                    for daemon in daemon_states[stream].keys():
                      response += "<daemon name='" + daemon + "'>" + str(daemon_states[stream][daemon]) + "</daemon>"
                    response += "</stream>"

                  for stream in client_streams:
                    response += "<stream id='" + str(stream) +"'>"
                    for daemon in daemon_states[stream].keys():
                      response += "<daemon name='" + daemon + "'>" + str(daemon_states[stream][daemon]) + "</daemon>"
                    response += "</stream>"
                  response += "</lmc_reply>"

                elif command == "host_status":
                  response = "<lmc_reply>"

                  for disk in disks.keys():
                    percent_full = 1.0 - (float(disks[disk]["available"]) / float(disks[disk]["size"]))
                    response += "<disk mount='" + disk +"' percent_full='"+str(percent_full)+"'>"
                    response += "<size units='MB'>" + disks[disk]["size"] + "</size>"
                    response += "<used units='MB'>" + disks[disk]["used"] + "</used>"
                    response += "<available units='MB'>" + disks[disk]["available"] + "</available>"
                    response += "</disk>"

                  for stream in smrbs.keys():
                    for key in smrbs[stream].keys():
                      smrb = smrbs[stream][key]
                      response += "<smrb stream='" + str(stream) + "' key='" + str(key) + "'>"
                      response += "<header_block nbufs='"+str(smrb['hdr']['nbufs'])+"'>"+ str(smrb['hdr']['full'])+"</header_block>"
                      response += "<data_block nbufs='"+str(smrb['data']['nbufs'])+"'>"+ str(smrb['data']['full'])+"</data_block>"
                      response += "</smrb>"
                  
                  response += "<system_load ncore='"+loads["ncore"]+"'>"
                  response += "<load1>" + loads["1min"] + "</load1>"
                  response += "<load5>" + loads["5min"] + "</load5>"
                  response += "<load15>" + loads["15min"] + "</load15>"
                  response += "</system_load>"

                  response += "<sensors>"
                  for sensor in sensors.keys():
                    response += "<metric name='" + sensor + "' units='"+sensors[sensor]["units"]+"'>" + sensors[sensor]["value"] + "</metric>"
                  response += "</sensors>"
                  
                  response += "</lmc_reply>"

                else:
                  response = "<lmc_reply>OK</lmc_reply>"

                self.log(2, "-> " + response)

                handle.send(response + "\r\n")

      counter -= 1


    def conclude (self):

      self.quit_event.set()

      for stream in self.client_streams:
        self.client_threads[stream].join()

      if self.server_thread:
        self.server_thread.join()

      script.log(1, "STOPPING SCRIPT")
      Daemon.conclude (self)


###############################################################################
#
if __name__ == "__main__":

  if len(sys.argv) != 2:
    print "ERROR: 1 command line argument expected"
    sys.exit(1)

  cfg = sys.argv[1]

  cfg_dir = os.environ.get('SPIP_ROOT') + "/share"
  cmd = "cp " + cfg_dir + "/" + cfg + "/*.cfg " + cfg_dir + "/"
  system (cmd, False)  

  hostname = getHostNameShort()

  script = LMCDaemon ("spip_lmc", hostname)
  state = script.configure (DAEMONIZE, DL, "lmc", "lmc")
  if state != 0:
    sys.exit(state)

  script.log(1, "STARTING SCRIPT")

  try:

    script.main()

  except:
    script.quit_event.set()

    script.log(-2, "exception caught: " + str(sys.exc_info()[0]))
    print '-'*60
    traceback.print_exc(file=sys.stdout)
    print '-'*60

  script.conclude()
  sys.exit(0)


