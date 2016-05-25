#!/usr/bin/env python

###############################################################################
#
#     Copyright (C) 2015 by Andrew Jameson
#     Licensed under the Academic Free License version 2.1
#
###############################################################################


import sys, traceback, os, threading
from time import sleep

from spip_gen import GenDaemon
from spip_smrb import SMRBDaemon
from spip.log_socket import LogSocket
from spip.utils.core import system_piped
from spip import Config

DAEMONIZE = False
DL = 2

#################################################################
# thread for reading .dada files into a ring buffer
class genThread (threading.Thread):

  def __init__ (self, cmd, pipe):
    threading.Thread.__init__(self)
    self.cmd = cmd
    self.pipe = pipe

  def run (self):
    rval = system_piped (self.cmd, self.pipe, True)
    return rval


class KAT7GenDaemon(GenDaemon):

  def __init__ (self, name, id):
    GenDaemon.__init__(self, name, str(id))

  ###############################################################################
  # Generate the 2 DADA data streams based on parameters in the XML message
  def gen_obs (self, fixed_config, message):

    self.log(1, "gen_obs: " + str(message))
    header = Config.readDictFromString(message)

    in_ids = self.cfg["RECEIVING_DATA_BLOCKS"].split(" ")
    db_prefix = self.cfg["DATA_BLOCK_PREFIX"]
    num_stream = self.cfg["NUM_STREAM"]
    pol1_key = SMRBDaemon.getDBKey (db_prefix, self.id, num_stream, in_ids[0])
    pol2_key = SMRBDaemon.getDBKey (db_prefix, self.id, num_stream, in_ids[1])

    # generate the header file to be use by GEN_BINARY
    header1_file = "/tmp/kat7_gen1_" + header["UTC_START"] + "." + self.id + ".header"
    header2_file = "/tmp/ket7_gen2_" + header["UTC_START"] + "." + self.id + ".header"

    header["HDR_VERSION"] = "1.0"
    fixed_config["NPOL"] = "1"
    fixed_config["BYTES_PER_SECOND"] = str(float(fixed_config["BYTES_PER_SECOND"])/2)
    fixed_config["RESOLUTION"] = str(float(fixed_config["RESOLUTION"])/2)
    #fixed_config["FILE_SIZE"] = str(float(fixed_config["FILE_SIZE"])/2)
    header["PICOSECONDS"] = "0";
    header["ADC_COUNTS"] = "0";

    # include the fixed configuration
    header.update(fixed_config)

    # rate in Gb/s
    transmit_rate = float(header["BYTES_PER_SECOND"]) / 1000000.0

    self.log(3, "gen_obs: writing header to " + header1_file + " and " + header2_file)
    Config.writeDictToCFGFile (header, header1_file)
    Config.writeDictToCFGFile (header, header2_file)

    stream_core = self.cfg["STREAM_GEN_CORE_" + str(self.id)]

    tobs = "60"
    if header["TOBS"] != "":
      tobs = header["TOBS"]

    cmd1 = "dada_junkdb -k " + pol1_key \
          + " -R " + str(transmit_rate) \
          + " -t " + tobs \
          + " -g " + header1_file
    self.binary_list.append (cmd1)

    cmd2 = "dada_junkdb -k " + pol2_key \
          + " -R " + str(transmit_rate) \
          + " -t " + tobs \
          + " -g " + header2_file
    self.binary_list.append (cmd2)

    sleep(1)

    log_pipe1 = LogSocket ("gen1_src", "gen1_src", str(self.id), "stream",
                        self.cfg["SERVER_HOST"], self.cfg["SERVER_LOG_PORT"],
                        int(DL))
    log_pipe2 = LogSocket ("gen2_src", "gen2_src", str(self.id), "stream",
                        self.cfg["SERVER_HOST"], self.cfg["SERVER_LOG_PORT"],
                        int(DL))

    log_pipe1.connect()
    log_pipe2.connect()

    sleep(1)

    pol1_thread = genThread (cmd1, log_pipe1.sock)
    pol2_thread = genThread (cmd2, log_pipe2.sock)

    # start processing threads
    self.log (1, "starting processing threads")
    pol1_thread.start()
    pol2_thread.start()

    # join processing threads
    self.log (2, "waiting for gen threads to terminate")
    rval = pol1_thread.join()
    self.log (2, "pol1 thread joined")
    if rval:
      self.log (-2, "pol1 thread failed")
      quit_event.set()

    rval = pol2_thread.join()
    self.log (2, "pol2 thread joined")
    if rval:
      self.log (-2, "pol2 thread failed")
      quit_event.set()

    log_pipe1.close ()
    log_pipe2.close ()


if __name__ == "__main__":

  if len(sys.argv) != 2:
    print "ERROR: 1 command line argument expected"
    sys.exit(1)

  # this should come from command line argument
  stream_id = sys.argv[1]

  script = KAT7GenDaemon ("kat7_gen", stream_id)
  state = script.configure (DAEMONIZE, DL, "gen", "gen")
  if state != 0:
    sys.exit(state)

  script.log(1, "STARTING SCRIPT")

  try:

    script.main ()

  except:

    script.quit_event.set()

    script.log(-2, "exception caught: " + str(sys.exc_info()[0]))
    print '-'*60
    traceback.print_exc(file=sys.stdout)
    print '-'*60

  script.log(1, "STOPPING SCRIPT")
  script.conclude()
  sys.exit(0)

