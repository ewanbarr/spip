#!/usr/bin/env python

###############################################################################
#  
#     Copyright (C) 2015 by Andrew Jameson
#     Licensed under the Academic Free License version 2.1
# 
###############################################################################


import sys, socket, select, traceback, errno, os
from time import sleep

from spip_read import ReadDaemon,diskdbThread
from spip_smrb import SMRBDaemon
from spip.log_socket import LogSocket

DAEMONIZE = False
DL = 2

class KAT7ReadDaemon(ReadDaemon):

  def __init__ (self, name, id):
    ReadDaemon.__init__(self, name, str(id))

  ###############################################################################
  # look for observations on disk that match
  def list_obs (self):
    self.log(2, "KAT7ReadDaemon::list_obs()")

    pol1_dir = "/data1/dada_test"
    pol2_dir = "/data2/dada_test"

    # observations are only valid if they exist in both pol1 and pol2 dirs
    # and have same path name

    subdirs = filter(os.path.isdir, [os.path.join(pol1_dir,f) for f in os.listdir(pol1_dir)])

    valid = "<observation_list pol1_dir='"+pol1_dir+"' pol2_dir='"+pol2_dir+"'>"

    for subdir in subdirs:
      self.log(3, "KAT7ReadDaemon::list_obs subdir="+subdir)

      if os.path.isdir(os.path.join(pol2_dir,subdir)):

        pol1_subdir = os.path.join(pol1_dir,subdir)
        pol2_subdir = os.path.join(pol2_dir,subdir)

        pol1_dadas = [file for file in os.listdir(pol1_subdir) if file.lower().endswith(".dada")]
        pol2_dadas = [file for file in os.listdir(pol2_subdir) if file.lower().endswith(".dada")]

        pol1_dadas.sort()
        pol2_dadas.sort()

        self.log(3, "KAT7ReadDaemon::list_obs pol1_dadas="+str(pol1_dadas))
        self.log(3, "KAT7ReadDaemon::list_obs pol2_dadas="+str(pol2_dadas))

        if len(pol1_dadas) > 0 and len(pol2_dadas) > 0:

          valid += "<observation name='" + subdir +"'>"

          valid += "<pol1 dir='" + pol1_subdir + "' nfiles='"+str(len(pol1_dadas))+"'>"
          for i in range(len(pol1_dadas)):
            valid += "<file index='"+str(i)+"'>" + pol1_dadas[i]+ "</file>"
          valid += "</pol1>"

          valid += "<pol2 dir='" + pol2_subdir + "' nfiles='"+str(len(pol2_dadas))+"'>"
          for i in range(len(pol2_dadas)):
            valid += "<file index='"+str(i)+"'>" + pol2_dadas[i]+ "</file>"
          valid += "</pol2>"

          valid += "</observation>"

    valid += "</observation_list>"

    return valid

  ###############################################################################
  # look for observations on disk that match
  def read_obs (self, xml):
    self.log(1, "KAT7ReadDaemon::read_obs()")
    self.log(1, "KAT7ReadDaemon::read_obs xml="+str(xml))

    # launch 2 threads, one for each pol and wait for them to finish
    pol1_dir = xml['pol1']['@dir']
    pol2_dir = xml['pol2']['@dir']

    pol1_nfiles =  xml['pol1']['@nfiles']
    pol2_nfiles =  xml['pol2']['@nfiles']

    pol1_files = []
    pol2_files = []

    for file in xml['pol1']['file']:
      pol1_files.append(pol1_dir + "/" + file['#text'])
    for file in xml['pol2']['file']:
      pol2_files.append(pol2_dir + "/" + file['#text'])

    # launch a thread for each dbdisk to read the .dada files into 
    # a dada buffer
    in_ids = self.cfg["RECEIVING_DATA_BLOCKS"].split(" ")
    db_prefix = self.cfg["DATA_BLOCK_PREFIX"]
    num_stream = self.cfg["NUM_STREAM"]
    pol1_key = SMRBDaemon.getDBKey (db_prefix, self.id, num_stream, in_ids[0])
    pol2_key = SMRBDaemon.getDBKey (db_prefix, self.id, num_stream, in_ids[1])

    log_host = self.cfg["SERVER_HOST"]
    log_port = int(self.cfg["SERVER_LOG_PORT"])

    # create a diskdb thread
    pol1_log_pipe   = LogSocket ("pol1_src", "pol1_src", str(self.id), "stream",
                                 log_host, log_port, int(DL))
    pol2_log_pipe   = LogSocket ("pol2_src", "pol2_src", str(self.id), "stream",
                                 log_host, log_port, int(DL))

    pol1_log_pipe.connect()
    pol2_log_pipe.connect()

    self.binary_list.append ("dada_diskdb -k " + pol1_key)
    self.binary_list.append ("dada_diskdb -k " + pol2_key)

    # create processing threads
    self.log (1, "creating processing threads")
    pol1_thread = diskdbThread (self, pol1_key, pol1_files, pol1_log_pipe.sock)
    pol2_thread = diskdbThread (self, pol2_key, pol2_files, pol2_log_pipe.sock)

    # start processing threads
    self.log (1, "starting processing threads")
    pol1_thread.start()
    pol2_thread.start()

    # join processing threads
    self.log (2, "waiting for diskdb threads to terminate")
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



if __name__ == "__main__":

  if len(sys.argv) != 2:
    print "ERROR: 1 command line argument expected"
    sys.exit(1)

  # this should come from command line argument
  stream_id = sys.argv[1]

  script = KAT7ReadDaemon ("kat7_read", stream_id)
  state = script.configure (DAEMONIZE, DL, "read", "read")
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
