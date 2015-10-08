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


import os, threading, sys, time, socket, select, signal, traceback
import errno, xmltodict
import spip
import spip_lmc_monitor as lmc_mon

SCRIPT = "spip_lmc"
DL     = 2

#################################################################
# clientThread
#
# launches client daemons for the specified stream
 
class clientThread (threading.Thread):

  def __init__ (self, stream_id, cfg, quit_event, states):
    threading.Thread.__init__(self)
    self.stream_id = stream_id
    self.quit_event = quit_event
    self.daemon_exit_wait = 10
    self.states = states

    if stream_id >= 0:
      self.daemon_list = cfg["CLIENT_DAEMONS"].split()
      self.control_dir = cfg["CLIENT_CONTROL_DIR"]
      self.prefix = "clientThread["+str(stream_id)+"] "
    else:
      self.daemon_list = cfg["SERVER_DAEMONS"].split()
      self.control_dir = cfg["SERVER_CONTROL_DIR"]
      self.prefix = "serverThread "

  def run (self):

    quit_event = self.quit_event
    process_suffix = ""
    file_suffix = ""
    if self.stream_id >= 0:
      process_suffix = " " + str(self.stream_id)
      file_suffix = "_" + str(self.stream_id)
    prefix = self.prefix
    daemons = {}

    spip.logMsg(2, DL, prefix + "thread running")

    try:

      ranked_daemons = self.daemon_list 
      for ranked_daemon in ranked_daemons:
        daemon, rank = ranked_daemon.split(":")
        if not rank in daemons:
          daemons[rank] = []
        daemons[rank].append(daemon)

      # start each of the daemons
      ranks = daemons.keys()
      ranks.sort()
      for rank in ranks:
        spip.logMsg(2, DL, prefix + "launching daemons of rank " + rank)
        for daemon in daemons[rank]:
          self.states[daemon] = False
          cmd = "python " + daemon + ".py" + process_suffix
          rval, lines = spip.system (cmd, 2 <= DL)
          if rval:
            for line in lines:
              spip.logMsg(-2, DL, prefix + line)
            quit_event.set() 
          else:
            for line in lines:
              spip.logMsg(2, DL, prefix + line)

        spip.logMsg(2, DL, prefix + "launched daemons of rank " + rank)
        time.sleep(1)

      spip.logMsg(2, DL, prefix + "launched all daemons")


      # here we would monitor the daemons in each stream and 
      # process control commands specific to the stream

      while (not quit_event.isSet()):

        for rank in ranks:
          for daemon in daemons[rank]: 
            cmd = "pgrep -f '^python " + daemon + ".py" + process_suffix + "'";
            rval, lines = spip.system (cmd, 3 <= DL)
            self.states[daemon] = (rval == 0)
            spip.logMsg(3, DL, prefix + daemon + ": " + str(self.states[daemon]))

        counter = 5
        while (not self.quit_event.isSet() and counter > 0):
          time.sleep(1)
          counter -= 1

      # stop each of the daemons in reverse order
      for rank in ranks[::-1]:

        # single all daemons in rank to exit
        for daemon in daemons[rank]:
          open(self.control_dir + "/" + daemon + file_suffix + ".quit", 'w').close()

        rank_timeout = self.daemon_exit_wait
        daemon_running = 1

        while daemon_running and rank_timeout > 0:
          daemon_running = 0
          for daemon in daemons[rank]:
            cmd = "pgrep -f '^python " + daemon + ".py" + process_suffix + "'"
            rval, lines = spip.system (cmd, 3 <= DL)
            if rval == 0 and len(lines) > 0:
              daemon_running = 1
              spip.logMsg(2, DL, prefix + "daemon " + daemon + " with rank " + 
                          str(rank) + " still running")
          if daemon_running:
            spip.logMsg(3, DL, prefix + "daemons " + "with rank " + str(rank) +
                        " still running")
            time.sleep(1)

        # if any daemons in this rank timed out, hard kill them
        if rank_timeout == 0:
          for daemon in daemons[rank]:
            cmd = "pkill -f ''^python " + daemon + ".py" + process_suffix + "'"
            rval, lines = spip.system (cmd, 3 <= DL)

        # remove daemon.quit files for this rank
        for daemon in daemons[rank]:
          os.remove (self.control_dir + "/" + daemon + file_suffix + ".quit")

      spip.logMsg(2, DL, prefix + " thread exiting")

    except:
      spip.logMsg(1, DL, prefix + "exception caught: " +
                  str(sys.exc_info()[0]))
      print '-'*60
      traceback.print_exc(file=sys.stdout)
      print '-'*60
#
################################################################$


#################################################################
# main
def main (argv):

  # read configuration file
  cfg = spip.getConfig()

  # get the configured hostname for matching server or client
  # streams
  hostname = spip.getHostNameShort()

  control_thread = []
  client_threads = {}
  server_thread = []

  log_file  = cfg["SERVER_LOG_DIR"] + "/" + SCRIPT + ".log"
  pid_file  = cfg["SERVER_CONTROL_DIR"] + "/" + SCRIPT + ".pid"
  quit_file = cfg["SERVER_CONTROL_DIR"] + "/"  + SCRIPT + ".quit"

  if os.path.exists(quit_file):
    sys.stderr.write("quit file existed at launch: " + quit_file + "\n")
    sys.exit(1)

  # become a daemon TODO uncomment
  # spip.daemonize(pid_file, log_file)

  try:

    spip.logMsg(1, DL, "STARTING SCRIPT")

    quit_event = threading.Event()

    def signal_handler(signal, frame):
      quit_event.set()

    signal.signal(signal.SIGINT, signal_handler)

    # start a control thread to handle quit requests
    control_thread = spip.controlThread(quit_file, pid_file, quit_event, DL)
    control_thread.start()

    # find matching client streams for this host
    client_streams = []
    for istream in range(int(cfg["NUM_STREAM"])):
      (req_host, beam_id, subband_id) = spip.getStreamConfig (istream, cfg)
      if (req_host == hostname):
        client_streams.append(istream)

    # find matching server stream
    server_streams = []
    if (cfg["SERVER_HOST"] == hostname):
      server_streams.append(0)

    daemon_states = {}

    # start a control thread for each stream
    for stream in client_streams:
      daemon_states[stream] = {}
      spip.logMsg(2, DL, "MAIN: client_thread["+str(stream)+"] = clientThread ("+str(stream)+")")
      client_threads[stream] = clientThread(stream, cfg, quit_event, daemon_states[stream])
      spip.logMsg(2, DL, "MAIN: client_thread["+str(stream)+"].start()")
      client_threads[stream].start()
      spip.logMsg(2, DL, "MAIN: client_thread["+str(stream)+"] started!")

    #for stream in server_streams:
    #  spip.logMsg(1, DL, "MAIN: client_thread[-1] = clientThread(-1)")
    #  server_thread = clientThread(-1, cfg, quit_event)
    #  spip.logMsg(1, DL, "MAIN: client_thread[-1].start()")
    #  server_thread.start()
    #  spip.logMsg(1, DL, "MAIN: client_thread[-1] started")

    # main thread
    disks_to_monitor = [cfg["CLIENT_DIR"]]

    # create socket for LMC commands
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind((hostname, int(cfg["LMC_PORT"])))
    sock.listen(5)

    can_read = [sock]
    can_write = []
    can_error = []
    timeout = 1
    hw_poll = 5
    counter = 0 

    # monitor / control loop
    while not quit_event.isSet():

      while (counter == 0):
        spip.logMsg(3, DL, "main: getDiskCapacity ()")
        rval, disks = lmc_mon.getDiskCapacity (disks_to_monitor, DL)
        spip.logMsg(3, DL, "main: " + str(disks))

        spip.logMsg(3, DL, "main: getLoads()")
        rval, loads = lmc_mon.getLoads (DL)
        spip.logMsg(3, DL, "main: " + str(loads))

        spip.logMsg(3, DL, "main: getSMRBCapacity(" + str(client_streams)+ ")")
        rval, smrbs = lmc_mon.getSMRBCapacity (client_streams, quit_event, DL)
        spip.logMsg(3, DL, "main: " + str(smrbs))

        spip.logMsg(3, DL, "main: getIPMISensors()")
        rval, sensors = lmc_mon.getIPMISensors (DL)
        spip.logMsg(3, DL, "main: " + str(sensors))

        counter = hw_poll

      spip.logMsg(3, DL, "main: calling select len(can_read)="+str(len(can_read)))
      timeout = 1
      did_read = []
      did_write = []
      did_error = []

      try:
        did_read, did_write, did_error = select.select(can_read, can_write, can_error, timeout)
      except select.error as e:
        quit_event.set()
      else:
        spip.logMsg(3, DL, "main: read="+str(len(did_read))+" write="+
                    str(len(did_write))+" error="+str(len(did_error)))

      if (len(did_read) > 0):
        for handle in did_read:
          if (handle == sock):
            (new_conn, addr) = sock.accept()
            spip.logMsg(1, DL, "main: accept connection from "+repr(addr))
            # add the accepted connection to can_read
            can_read.append(new_conn)
            # new_conn.send("Welcome to the LMC interface\r\n")

          # an accepted connection must have generated some data
          else:
            try:
              raw = handle.recv(4096)
            except socket.error, e:
              if e.errno == errno.ECONNRESET:
                spip.logMsg(1, DL, "commandThread: closing connection")
                handle.close()
                for i, x in enumerate(can_read):
                  if (x == handle):
                    del can_read[i]
              else:
                raise e
            else: 
              message = raw.strip()
              spip.logMsg(1, DL, "commandThread: message='" + message+"'")
              xml = xmltodict.parse(message)

              command = xml["lmc_cmd"]["command"]
              if command == "daemon_status":
                response = ""
                response += "<lmc_reply>"
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

              spip.logMsg(2, DL, "-> " + response)

              handle.send(response + "\r\n")

      counter -= 1

  except:
    spip.logMsg(-2, DL, "main: exception caught: " + str(sys.exc_info()[0]))
    print '-'*60
    traceback.print_exc(file=sys.stdout)
    print '-'*60
    quit_event.set()

  # join threads
  spip.logMsg(2, DL, "main: joining control thread")
  if control_thread:
    control_thread.join()

  for stream in client_streams:
    client_threads[stream].join()

  if server_thread:
    server_thread.join()

  spip.logMsg(1, DL, "STOPPING SCRIPT")

if __name__ == "__main__":
  if len(sys.argv) != 2:
    print "ERROR: 1 command line argument expected"
    sys.exit(1)

  main (sys.argv)
  sys.exit(0)
