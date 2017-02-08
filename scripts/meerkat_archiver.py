#!/usr/bin/env python

###############################################################################
#  
#     Copyright (C) 2017 by Andrew Jameson
#     Licensed under the Academic Free License version 2.1
# 
###############################################################################

import logging
import sys, os, traceback
from time import sleep

from spip.daemons.bases import BeamBased,ServerBased
from spip.daemons.daemon import Daemon
from spip.config import Config

import katsdptransfer

DAEMONIZE = True
DL = 1


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

  def extractKey(self, dict, key):
    if key in dict.keys():
      return dict[key]
    else:
      return ""

  def generateObsInfoDat (self, finished_subdir, completed_subdir):

    obs_results_file = self.finished_dir + "/" + finished_subdir + "/obs.results"
    obs_header_file = self.completed_dir + "/" + completed_subdir + "/obs.header"
    obs_info_dat_file = self.completed_dir + "/" + completed_subdir + "/obs_info.dat"
   
    if not os.path.exists (obs_info_dat_file):

      self.log (2, "MeerKATArchiverDaemon::generateObsInfoDat creating obs_info.dat")

      obs_results = Config.readCFGFileIntoDict(obs_results_file)
      obs_header = Config.readCFGFileIntoDict(obs_header_file)
      obs_info_dat = {}
  
      obs_info_dat["observer"] = self.extractKey(obs_header ,"OBSERVER")
      obs_info_dat["program_block_id"] = self.extractKey(obs_header, "PROGRAM_BLOCK_ID")
      obs_info_dat["targets"] = "['" + self.extractKey(obs_header,"SOURCE") + "']"
      obs_info_dat["mode"] = self.extractKey(obs_header,"MODE")
      obs_info_dat["sb_id_code"] = self.extractKey(obs_header,"SCHEDULE_BLOCK_ID")
      obs_info_dat["target_duration"] = self.extractKey(obs_results, "length")
      obs_info_dat["target_snr"] = self.extractKey(obs_results, "snr")
      obs_info_dat["proposal_id"] = self.extractKey(obs_header, "PROPOSAL_ID")
      obs_info_dat["description"] = self.extractKey(obs_header, "DESCRIPTION")
      obs_info_dat["backend_args"] = "TBD"
      obs_info_dat["experiment_id"] = self.extractKey(obs_header,"EXPERIMENT_ID")

      Config.writeDictToColonSVFile(obs_info_dat, obs_info_dat_file)

    else:
      self.log (2, "MeerKATArchiverDaemon::generateObsInfoDat obs_info.dat existed")

    return ("ok", "")

  def main (self):

    self.ftp_server = "hdd-pod1.kat.ac.za"
    self.ftp_username = "kat"
    self.ftp_password = "kat"
    self.local_path = self.completed_dir
    self.remote_path = "staging"

    self.log (2, "main: creating AuthenticatedFtpTransfer")

    self.ftp_agent = katsdptransfer.ftp_transfer.AuthenticatedFtpTransfer (server=self.ftp_server, username=self.ftp_username, password=self.ftp_password, local_path=self.local_path,remote_path=self.remote_path, tx_md5=False)

    while not self.quit_event.isSet():

      # look for observations that have been completed archived / beam / utc / source
      cmd = "find " + self.completed_dir + " -mindepth 5 -maxdepth 5 -type f -name 'obs.finished' -mmin +1 | sort"
      rval, fin_files = self.system(cmd, 2)
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

          finished_subdir = beam + "/" + utc + "/" + source
          completed_subdir = beam + "/" + utc + "/" + source + "/" + cfreq

          # form the obs.dat file that is parsed during ingest
          (rval, response) = self.generateObsInfoDat (finished_subdir, completed_subdir)

          # name of the directory to transfer (flat)
          ftp_utc = utc.replace(":","").replace("-","")
          ftp_source = source.replace("+","p").replace("-","m")
          ftp_dir = "PTUSE_" + beam + "_" + ftp_utc + "_" + ftp_source

          self.log (2, "main: ftp_dir=" + ftp_dir)

          cmd = "find " + self.completed_dir + "/" + completed_subdir + " -mindepth 1 -maxdepth 1 -type f -printf '%f\n' | grep -v obs.finished | sort -n"
          rval, files = self.system(cmd, 3)
          if rval:
            self.log (-1, "main: find command failed: " + files[0])
            sleep(1)
          else:

            self.ftp_agent.remote_path = self.remote_path + "/" + ftp_dir + ".writing"
            self.log (2, "main: ftp_agent.remote_path=" + self.ftp_agent.remote_path)

            self.log (2, "main: creating ftp_agent.connecting to " + self.ftp_server)
            self.ftp_agent.connect()
          
            self.log (2, "main: transferring " + str(len(files)) + " files")

            self.log (1, beam + "/" + utc + "/"  + source + " transferring")

            for file in files:

              self.ftp_agent.local_path = self.completed_dir + "/" + completed_subdir
              self.log (3, "main: ftp_agent.local_path=" + self.ftp_agent.local_path)
              self.log (3, "main: ftp_agent.remote_path=" + self.ftp_agent.remote_path)
              self.log (3, "main: ftp_agent.put(" +file +")")

              self.ftp_agent.put (file)

            self.log (2, "main: ftp_agent.rename remote path, removing .writing")
            self.ftp_agent.ftp.rename (self.remote_path + "/" + ftp_dir + ".writing", self.remote_path + "/" + ftp_dir)

            self.log (2, "main: ftp_agent.close()")
            self.ftp_agent.close()

            # now move this observation from completed to transferred
            cmd = "mkdir -p " + self.transferred_dir + "/" + beam
            rval, lines = self.system(cmd, 2)
            cmd = "mv " + self.completed_dir + "/" + beam + "/" + utc + " " + self.transferred_dir + "/" + beam + "/"
            rval, lines = self.system(cmd, 2)

            self.log (1, beam + "/" + utc + "/"  + source + " transferred")

      to_sleep = 10
      self.log (2, "main: sleeping " + str(to_sleep) + " seconds")
      while to_sleep > 0 and not self.quit_event.isSet():
        sleep(1)
        to_sleep -= 1



###############################################################################
# Server based implementation
class MeerKATArchiverServerDaemon(MeerKATArchiverDaemon, ServerBased):

  def __init__ (self, name):
    MeerKATArchiverDaemon.__init__(self, name, "-1")
    ServerBased.__init__(self, self.cfg)

  def configure (self,become_daemon, dl, source, dest):

    MeerKATArchiverDaemon.configure (self, become_daemon, dl, source, dest)

    self.completed_dir   = self.cfg["SERVER_FOLD_DIR"] + "/archived"
    self.finished_dir   = self.cfg["SERVER_FOLD_DIR"] + "/finished"
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
    self.finished_dir   = self.cfg["CLIENT_FOLD_DIR"] + "/finished"
    self.transferring_dir = self.cfg["CLIENT_FOLD_DIR"] + "/send"
    self.transferred_dir = self.cfg["CLIENT_FOLD_DIR"] + "/sent"

    bid = self.cfg["BEAM_" + str(self.beam_id)]
    self.beams.append(bid)
    return 0
    
###############################################################################
# main

if __name__ == "__main__":

  # logging.basicConfig(filename='example.log', filemode='w', level=logging.DEBUG)

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
