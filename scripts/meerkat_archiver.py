#!/usr/bin/env python

###############################################################################
#  
#     Copyright (C) 2017 by Andrew Jameson
#     Licensed under the Academic Free License version 2.1
# 
###############################################################################

import logging
import sys, traceback
from time import sleep

from spip.daemons.bases import BeamBased,ServerBased
from spip.daemons.daemon import Daemon
from spip.config import Config

import katsdptransfer



DAEMONIZE = False
DL = 2


###############################################################################
#  Generic Implementation
class MeerKATArchiverDaemon(Daemon):

  def __init__ (self, name, id):
    Daemon.__init__(self, name, str(id))
    self.beams = []

    self.ftp_server = "sun-store.kat.ac.za"
    self.ftp_username = "kat"
    self.ftp_password = "kat"

  def configure (self, become_daemon, dl, source, dest):
    Daemon.configure (self, become_daemon, dl, source, dest)
    return 0

  def generateObsInfoDat (self, path):

    obs_header_file = self.completed_dir + "/" + path + "/obs.header"
    obs_info_dat_file = self.completed_dir + "/" + path + "/obs_info.dat"
    
    obs_header = Config.readCFGFileIntoDict(obs_header_file)
    obs_info_dat = {}
    obs_info_dat["observer"] = obs_header["OBSERVER"]
    obs_info_dat["program_block_id"] = "TBD"
    obs_info_dat["targets"] = "['" + obs_header["SOURCE"] + "']"
    obs_info_dat["sb_id_code"] = obs_header["SCHEDULE_BLOCK_ID"]
    obs_info_dat["target_duration"] = "TBD"
    obs_info_dat["proposal_id"] = "TBD"
    obs_info_dat["description"] = obs_header["DESCRIPTION"]
    obs_info_dat["backend_args"] = "TBD"
    obs_info_dat["experiment_id"] = obs_header["EXPERIMENT_ID"]

    Config.writeDictToColonSVFile(obs_info_dat, obs_info_dat_file)

    return ("ok", "")

  def main (self):

    self.ftp_server = "hdd-pod1.kat.ac.za"
    self.ftp_username = "kat"
    self.ftp_password = "kat"
    self.local_path = self.completed_dir
    self.remote_path = "staging"

    self.log (1, "main: creating AuthenticatedFtpTransfer")

    self.ftp_agent = katsdptransfer.ftp_transfer.AuthenticatedFtpTransfer (server=self.ftp_server, username=self.ftp_username, password=self.ftp_password, local_path=self.local_path,remote_path=self.remote_path, tx_md5=False)


    while not self.quit_event.isSet():

      # look for observations that have been completed archived / beam / utc / source
      cmd = "find " + self.completed_dir + " -mindepth 5 -maxdepth 5 -type f -name 'obs.finished' -mmin +1"
      rval, fin_files = self.system(cmd, 1)
      if rval:
        self.log (-1, "main: find command failed: " + fin_files[0])
        sleep(1)
      else:

        for path in fin_files:

          if self.quit_event.isSet():
            continue
 
          # strip dir prefix
          subpath = path [(len(self.completed_dir)+1):] 

          (beam, utc, source, cfreq, file) = subpath.split("/")

          subdir = beam + "/" + utc + "/" + source + "/" + cfreq

          # form the obs.dat file that is parsed during ingest
          (rval, response) = self.generateObsInfoDat(subdir)

          # name of the directory to transfer (flat)
          ftp_utc = utc.replace(":","").replace("-","")
          ftp_source = source.replace("+","p").replace("-","m")
          ftp_dir = "PTUSE_" + beam + "_" + ftp_utc + "_" + ftp_source

          self.log (2, "main: ftp_dir=" + ftp_dir)

          cmd = "find " + self.completed_dir + "/" + subdir + " -mindepth 1 -maxdepth 1 -type f -printf '%f\n' | grep -v obs.finished"
          rval, files = self.system(cmd, 3)
          if rval:
            self.log (-1, "main: find command failed: " + files[0])
            sleep(1)
          else:

            self.ftp_agent.remote_path = self.remote_path + "/" + ftp_dir
            self.log (1, "main: ftp_agent.remote_path=" + self.ftp_agent.remote_path)

            self.log (1, "main: creating ftp_agent.connect()")
            self.ftp_agent.connect()
          
            for file in files:

              self.ftp_agent.local_path = self.completed_dir + "/" + subdir
              self.log (2, "main: ftp_agent.local_path=" + self.ftp_agent.local_path)
              self.log (2, "main: ftp_agent.remote_path=" + self.ftp_agent.remote_path)
              self.log (2, "main: ftp_agent.put(" +file +")")

              self.ftp_agent.put (file)

            self.log (1, "main: ftp_agent.close()")
            self.ftp_agent.close()

            # now move this observation from completed to transferred
            cmd = "mkdir -p " + self.transferred_dir + "/" + beam
            rval, lines = self.system(cmd, 2)
            cmd = "mv " + self.completed_dir + "/" + beam + "/" + utc " " + self.transferred_dir + "/" + beam + "/"
            rval, lines = self.system(cmd, 2)

      self.quit_event.set()

      to_sleep = 10
      while to_sleep > 0 and not self.quit_event.isSet():
        sleep(1)
        to_sleep -= 1

    self.log (2, "main: closing ftp connection")
    self.ftp_agent.close()

###############################################################################
# Server based implementation
class MeerKATArchiverServerDaemon(MeerKATArchiverDaemon, ServerBased):

  def __init__ (self, name):
    MeerKATArchiverDaemon.__init__(self, name, "-1")
    ServerBased.__init__(self, self.cfg)

  def configure (self,become_daemon, dl, source, dest):

    MeerKATArchiverDaemon.configure (self, become_daemon, dl, source, dest)

    self.completed_dir   = self.cfg["SERVER_FOLD_DIR"] + "/archived"
    self.transferring_dir   = self.cfg["SERVER_FOLD_DIR"] + "/send"
    self.transferred_dir = self.cfg["SERVER_FOLD_DIR"] + "/sent"

    for i in range(int(self.cfg["NUM_BEAM"])):
      bid = self.cfg["BEAM_" + str(i)]
      self.beams.append(bid)
    return 0


###############################################################################
# Beam based implementation
class MeerKATArchiverBeamDaemon (MeerKATArchiverDaemon, BeamBased):

  def __init__ (self, name, id):
    MeerKATArchiverDaemon.__init__(self, name, str(id))
    BeamBased.__init__(self, str(id), self.cfg)

  def configure (self, become_daemon, dl, source, dest):

    self.log(1, "MeerKATArchiverBeamDaemon::configure()")
    MeerKATArchiverDaemon.configure(self, become_daemon, dl, source, dest)

    self.completed_dir   = self.cfg["CLIENT_FOLD_DIR"] + "/archived"
    self.transferring_dir = self.cfg["CLIENT_FOLD_DIR"] + "/send"
    self.transferred_dir = self.cfg["CLIENT_FOLD_DIR"] + "/sent"

    bid = self.cfg["BEAM_" + str(self.beam_id)]
    self.beams.append(bid)
    return 0
    
###############################################################################
# main

if __name__ == "__main__":

  logging.basicConfig(filename='example.log', filemode='w', level=logging.DEBUG)

  if len(sys.argv) != 2:
    print "ERROR: 1 command line argument expected"
    sys.exit(1)

  # this should come from command line argument
  beam_id = sys.argv[1]

  script = []
  if int(beam_id) == -1:
    script = MeerKATArchiverServerDaemon ("meerkat_archiver")
  else:
    script = MeerKATArchiverBeamDaemon ("meerkat_archiver", beam_id)

  state = script.configure (DAEMONIZE, DL, "archiver", "archiver")
  if state != 0:
    sys.exit(state)

  script.log(2, "STARTING SCRIPT")

  try:
    script.main ()

  except:
    script.quit_event.set()
    script.log(-2, "exception caught: " + str(sys.exc_info()[0]))
    print '-'*60
    traceback.print_exc(file=sys.stdout)
    print '-'*60

  script.log(2, "STOPPING SCRIPT")
  script.conclude()
  sys.exit(0)
