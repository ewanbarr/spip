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
import spip
import spip_lmc_monitor as lmc_mon

SCRIPT = "spip_lmc"
DL     = 2

#################################################################
# clientThread
#
# launches client daemons for the specified stream
 
class clientThread (threading.Thread):

  def __init__ (self, stream_id, cfg, quit_event):
    threading.Thread.__init__(self)
    self.stream_id = stream_id
    self.quit_event = quit_event
    self.daemon_exit_wait = 10

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
    stream_id = self.stream_id
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
      print "ranks="+str(ranks)+"\n"
      for rank in ranks:
        spip.logMsg(2, DL, prefix + "launching daemons of rank " + rank)
        for daemon in daemons[rank]:
          cmd = "python " + daemon + ".py " + str(stream_id)
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

      # daemon_states = {}

      # here we would monitor the daemons in each stream and 
      #  process control commands specific to the stream

      while (not quit_event.isSet()):

        for rank in ranks:
          for daemon in daemons[rank]: 
            cmd = "pgrep -f '^python " + daemon + ".py " + stream_id + "'";
            rval, lines = spip.system (cmd, 2 <= DL)
            daemon_states[daemon] = (rval == 0)
        
        spip.logMsg(2, DL, prefix + daemon_states)

        counter = 5
        while (not self.quit_event.isSet() and counter > 0):
          time.sleep(1)

      # stop each of the daemons in reverse order
      for rank in ranks[::-1]:

        # single all daemons in rank to exit
        for daemon in daemons[rank]:
          open(self.control_dir + "/" + daemon + ".quit", 'w').close()

        rank_timeout = self.daemon_exit_wait
        daemon_running = 1

        while daemon_running and rank_timeout > 0:
          daemon_running = 0
          for daemon in daemons[rank]:
            cmd = "pgrep -f '^python " + daemon + ".py " + stream_id + "'"
            rval, lines = spip.system (cmd, 2 <= DL)
            if rval == 0 and len(lines) > 0:
              daemon_running = 1
              spip.logMsg(2, DL, prefix + "daemon " + daemon + " with rank " + 
                          str(rank) + " still running")
          if daemon_running:
            spip.logMsg(2, DL, prefix + "daemons " + "with rank " + str(rank) +
                        " still running")
            time.sleep(1)

        # if any daemons in this rank timed out, hard kill them
        if rank_timeout == 0:
          for daemon in daemons[rank]:
            cmd = "pkill -f ''^python " + daemon + ".py " + stream_id + "'"
            rval, lines = spip.system (cmd, 2 <= DL)

        # remove daemon.quit files for this rank
        for daemon in daemons[rank]:
          os.remove (self.control_dir + "/" + daemon + ".quit")

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

  log_file  = cfg["SERVER_LOG_DIR"] + "/" + SCRIPT + ".log"
  pid_file  = cfg["SERVER_CONTROL_DIR"] + "/" + SCRIPT + ".pid"
  quit_file = cfg["SERVER_CONTROL_DIR"] + "/"  + SCRIPT + ".quit"

  if os.path.exists(quit_file):
    sys.stderr.write("quit file existed at launch: " + quit_file)
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

    # start a control thread for each stream
    client_threads = {}
    for stream in client_streams:
      client_threads[stream] = clientThread(stream, cfg, quit_event)
      client_threads[stream].run()

    for stream in server_streams:
      server_thread = serverThread()
      server_thread.run()

    # main thread
    disks_to_monitor = [cfg["CLIENT_LOCAL_DIR"]]

    # monitor / control loop
    while not quit_event.isSet():

      if False:
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

      counter = 5
      while (not quit_event.isSet() and counter > 0):
        time.sleep(1)
        counter -= 1

  except:
    spip.logMsg(-2, DL, "main: exception caught: " + str(sys.exc_info()[0]))
    print '-'*60
    traceback.print_exc(file=sys.stdout)
    print '-'*60
    quit_event.set()

  # join threads
  spip.logMsg(2, DL, "main: joining control thread")
  if (control_thread):
    control_thread.join()

  spip.logMsg(1, DL, "STOPPING SCRIPT")

if __name__ == "__main__":
  if len(sys.argv) != 2:
    print "ERROR: 1 command line argument expected"
    sys.exit(1)

  main (sys.argv)
  sys.exit(0)
