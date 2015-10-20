#!/usr/bin/env python

##############################################################################
#  
#     Copyright (C) 2015 by Andrew Jameson
#     Licensed under the Academic Free License version 2.1
# 
###############################################################################

import os, sys, socket, select, signal, traceback
from time import sleep

from spip.daemons.bases import BeamBased,ServerBased
from spip.daemons.daemon import Daemon
from spip.threads.reporting_thread import ReportingThread
from spip.utils import times,sockets

DAEMONIZE = False
DL        = 2

class RepackReportingThread(ReportingThread):

  def __init__ (self, script, id):
    host = sockets.getHostNameShort()
    port = int(script.cfg["STREAM_REPACK_PORT"])
    if id >= 0:
      port += int(id)
    ReportingThread.__init__(self, script, host, port)

  def parse_message (self, request):
    self.script.log (0, "RepackReportingThread::parse_message: " + str(request))

    xml = ""
    req = request["repack_request"]

    if req["type"] == "state":

      self.script.log (0, "RepackReportingThread::parse_message: preparing state response")
      xml = "<repack_state>"

      for beam in self.script.beams:

        self.script.log (0, "RepackReportingThread::parse_message: preparing state for beam: " + str(beam))
        xml += "<beam id='" + str(beam) + "'>"

        self.script.log (0, "RepackReportingThread::parse_message: keys="+str(self.script.results[beam].keys()))

        if self.script.results[beam]["valid"]:

          self.script.log (0, "RepackReportingThread::parse_message: beam " + str(beam) + " is valid!")
          xml += "<source>"
          xml += "<name epoch='J2000'></name>"
          xml += "</source>"

          xml += "<observation>"
          xml += "<start units='datetime'>" + self.script.results[beam]["utc_start"] + "</start>"
          xml += "<length units='seconds'>" + self.script.results[beam]["length"] + "</length>"
          xml += "<snr>" + self.script.results[beam]["snr"] + "</snr>"
          xml += "</observation>"

          xml += "<plot type='freq_vs_phase'/>"
          xml += "<plot type='time_vs_phase'/>"
          xml += "<plot type='flux_vs_phase'/>"
      

        xml += "</beam>"

      xml += "</repack_state>"
      self.script.log (0, "RepackReportingThread::parse_message: returning " + str(xml))
    
      return xml + "\r\n"

    elif req["type"] == "plot":
     
      self.script.log (0, "RepackReportingThread::parse_message: beam=" + \
                          req["beam"] + " plot=" + req["plot"]) 
      self.script.log (0, "RepackReportingThread::parse_message: results[" + \
                          req["beam"] + "][valid]=" + str(self.script.results[req["beam"]]["valid"]))
      if self.script.results[req["beam"]]["valid"]:
        bin_data = self.script.results[req["beam"]][req["plot"]]
        self.script.log (0, "RepackReportingThread::parse_message: len(bin_data)="+str(len(bin_data)))
        return self.script.results[req["beam"]][req["plot"]]
      else:
        # still return if the timestamp is recent
        # TODO return image with "no valid data" or similar
        return 0

    else:

      xml += "<repack_state>"
      xml += "<error>Invalid request</error>"
      xml += "</repack_state>"

      return xml

class RepackDaemon(Daemon):

  def __init__ (self, name, id):
    Daemon.__init__(self, name, str(id))

    self.beams = []
    self.subbands = []
    self.results = {}

  #################################################################
  # main
  #       id >= 0   process folded archives from a stream
  #       id == -1  process folded archives from all streams
  def main (self):

    archives_glob = "*.ar"

    self.log (2, "main: beams=" + str(self.beams))

    # archives stored in directory structure
    #  beam / utc_start / source / cfreq / "fold"

    # summary data stored in
    #  beam / utc_start / source / freq.sum
    out_cfreq = 0

    self.log (2, "main: stream_id=" + str(self.id))

    while (not self.quit_event.isSet()):

      # check each beam for folded archives to process    
      for beam in self.beams:

        beam_dir = self.processing_dir + "/" + beam
        self.log (2, "main: beam=" + beam + " beam_dir=" + beam_dir)

        # get a list of all the recent observations
        cmd = "find " + beam_dir + " -mindepth 2 -maxdepth 2 -type d"
        self.log (2, "main: " + cmd)
        rval, observations = self.system (cmd, 3)

        # for each observation      
        for observation in observations:
   
          # strip prefix 
          observation = observation[(len(beam_dir)+1):]

          (utc, source) = observation.split("/")

          obs_dir = beam_dir + "/" + observation
          out_dir = self.archived_dir + "/" + beam + "/" + utc + "/" + source + "/" + str(out_cfreq)

          self.log (2, "main: chekcing out_dir=" + out_dir)
          if not os.path.exists(out_dir):
            os.makedirs(out_dir, 0755)

          # if we have only 1 sub-band, then files can be processed immediately
          archives = {}
          for subband in self.subbands:
            self.log (2, "processing subband=" + str(subband))
            
            cmd = "find " + obs_dir + "/" + subband["cfreq"] + " -mindepth 1 -maxdepth 1 " + \
                  "-type f -name '" + archives_glob + "' -printf '%f\\n'"
            
            self.log (2, "main: " + cmd)
            rval, files = self.system (cmd)
            sleep(2)

            for file in files:
              if not file in archives:
                archives[file] = 0
              archives[file] += 1

          # if a file meets the subband count it is ripe for processing
          files = archives.keys()
          files.sort()

          for file in files:

            self.log (2, "main: processing file=" + file)

            if archives[file] == len(self.subbands):
              if len(self.subbands) > 1:
                self.log (2, "main: process_subband()")
                (rval, response) = self.process_subband (obs_dir, out_dir, source, file)
                if rval:
                  self.log (-1, "failed to process sub-bands for " + file + ": " + response)
              else:
                input_file  = obs_dir  + "/" + self.subbands[0]["cfreq"] + "/" + file
                output_file = out_dir + "/" + file
                self.log (2, "main: process_archive() "+ input_file)
                (rval, response) = self.process_archive (obs_dir, input_file, output_file, source)
                if rval:
                  self.log (-1, "failed to process " + file + ": " + response)

          if len(files) > 0:
            # now process the sum files to produce plots etc
            self.log (2, "main: process_observation("+beam+","+utc+","+source+","+obs_dir+")")
            (rval, response) = self.process_observation (beam, utc, source, obs_dir)
            if rval:
              self.log (-1, "failed to process observation: " + response)

          # if the proc has marked this observation as finished
          all_finished = True

          # perhaps a file was produced whilst the previous list was being processed,
          # do another pass
          if len(files) > 0:
            all_finished = False

          for subband in self.subbands:
            if not os.path.exists(obs_dir + "/" + subband["cfreq"] + "/obs.finished"):
              all_finished = False
          
          if all_finished: 
            self.log (2, "main: " + observation + " finished")

            fin_parent_dir = self.finished_dir + "/" + beam + "/" + utc
            if not os.path.exists(fin_parent_dir):
              os.makedirs(fin_parent_dir, 0755)

            fin_dir = self.finished_dir + "/" + beam + "/" + utc + "/" + source
            self.log (2, "main: finalise_observation("+obs_dir+")")
            (rval, response) = self.finalise_observation (beam, obs_dir, fin_dir)
            if rval:
              self.log (-1, "failed to finalise observation: " + response)
            else:
              for subband in self.subbands: 
                os.remove (fin_dir + "/" + subband["cfreq"] + "/obs.finished")
                os.removedirs (fin_dir + "/" + subband["cfreq"])

      self.log (2, "sleep(1)")
      sleep(1)

  #
  # process and file in the directory, adding file to 
  #
  def process_archive (self, in_dir, input_file, output_file, source):

    self.log (2, "process_archive() input_file=" + input_file)

    freq_file   = in_dir + "/freq.sum"
    time_file   = in_dir + "/time.sum"

    # copy the input file to output dir
    cmd = "cp " + input_file + " " + output_file
    rval, lines = self.system (cmd)

    # add the archive to the 
    if not os.path.exists(freq_file):
      cmd = "cp " + input_file + " " + freq_file
      rval, lines = self.system (cmd)
      if rval:
        return (rval, "failed to copy archive to freq.sum")
    else:
      cmd = "psradd -T -o " + freq_file + " " + freq_file + " " + input_file
      rval, lines = self.system (cmd)
      if rval:
        return (rval, "failed add archive to freq.sum")

    cmd = "pam -m -F " + input_file
    rval, lines = self.system (cmd)
    if rval:
      return (rval, "failed add Fscrunch archive")

    if not os.path.exists(time_file):
      try:
        os.rename (input_file, time_file)
      except OSError, e:
        return (-1, "failed rename Fscrunched archive to time.sum: " + str(e))
    else:
      cmd = "psradd -o " + time_file + " " + time_file + " " + input_file
      rval, lines = self.system (cmd)
      if rval:
        return (rval, "failed add Fscrunched archive to time.sum")
      try:
        os.remove (input_file)
      except OSError, e:
        return (-1, "failed remove Fscrunched archive")

    return (0, "")

  #
  # process all sub-bands for the same archive
  #
  def process_subband (self, in_dir, out_dir, source, file):

    output_file = out_dir + "/" + file
    interim_file = "/dev/shm/" + file
    input_files = in_dir + "/*/" + file

    cmd = "psradd -R -o " + interim_file + " " + input_files
    rval, observations = self.system (cmd)
    if rval:
      return (rval, "failed to add sub-band archives to interim file")

    (rval, response) = self.process_archive (in_dir, interim_file, output_file, source)
    if rval:
      return (rval, "process_archive failed: " + response)
    
    # remove in the input sub-banded files
    cmd = "rm -f " + input_files
    rval, lines = self.system (cmd)
    if rval:
      return (rval, "failed to delete input files")

    return (0, "")

  def process_observation (self, beam, utc, source, in_dir):

    freq_file   = in_dir + "/freq.sum"
    time_file   = in_dir + "/time.sum"

    timestamp = times.getCurrentTime() 

    cmd = "psrplot -p freq " + freq_file + " -D -/png"
    rval, freq_raw = self.system_raw (cmd)
    if rval < 0:
      return (rval, "failed to create freq plot")

    cmd = "psrplot -p time " + time_file + " -D -/png"
    rval, time_raw = self.system_raw (cmd)
    if rval < 0:
      return (rval, "failed to create time plot")

    cmd = "psrplot -p flux -jF " + freq_file + " -D -/png"
    rval, flux_raw = self.system_raw (cmd)
    if rval < 0:
      return (rval, "failed to create time plot")

    cmd = "psrstat -jFDp -c snr " + freq_file + " | awk -F= '{printf(\"%5.1f\",$2)}'"
    rval, lines = self.system (cmd)
    if rval < 0:
      return (rval, "failed to extract snr from freq.sum")
    snr = lines[0]

    cmd = "psrstat -c length " + time_file + " | awk -F= '{printf(\"%5.1f\",$2)}'"
    rval, lines = self.system (cmd)
    if rval < 0:
      return (rval, "failed to extract time from time.sum")
    length = lines[0]

    self.results[beam]["utc_start"] = utc
    self.results[beam]["source"] = source
    self.results[beam]["freq_plot"] = freq_raw
    self.results[beam]["time_plot"] = time_raw
    self.results[beam]["flux_plot"] = flux_raw
    self.results[beam]["timestamp"] = timestamp
    self.results[beam]["snr"] = snr
    self.results[beam]["length"] = snr
    self.results[beam]["valid"] = True

    return (0, "")

  def finalise_observation (self, beam, obs_dir, fin_dir):

    # write the most recent images disk for long term storage
    timestamp = times.getCurrentTime()
    
    self.log (2, "finalise_observation: beam=" + beam + " timestamp=" + \
              timestamp + " valid=" + str(self.results[beam]["valid"]))

    if (self.results[beam]["valid"]):
    
      freq_plot = obs_dir + "/" + timestamp + ".freq.png"
      time_plot = obs_dir + "/" + timestamp + ".time.png"

      fptr = open (freq_plot, "wb")
      fptr.write(self.results[beam]["freq_plot"])
      fptr.close()

      fptr = open (time_plot, "wb")
      fptr.write(self.results[beam]["time_plot"])
      fptr.close()

      # indicate that the beam is no longer valid now that the 
      # observation has finished
      self.results[beam]["valid"] = False

    # simply move the observation to the finished directory
    try:
      os.rename (obs_dir, fin_dir)
    except OSError, e:
      return (1, "failed to rename obs_dir to fin_dir")

    return (0, "")

class RepackServerDaemon (RepackDaemon, ServerBased):

  def __init__ (self, name):
    RepackDaemon.__init__(self,name, "-1")
    ServerBased.__init__(self, self.cfg)

  def configure (self,become_daemon, dl, source, dest):

    Daemon.configure (self, become_daemon, dl, source, dest)

    self.processing_dir = self.cfg["SERVER_FOLD_DIR"] + "/processing"
    self.finished_dir   = self.cfg["SERVER_FOLD_DIR"] + "/finished"
    self.archived_dir   = self.cfg["SERVER_FOLD_DIR"] + "/archived"

    for i in range(int(self.cfg["NUM_BEAM"])):
      bid = self.cfg["BEAM_" + str(i)]
      self.beams.append(bid)
      self.results[bid] = {}
      self.results[bid]["valid"] = False

    for i in range(int(self.cfg["NUM_SUBBAND"])):
      (cfreq , bw, nchan) = self.cfg["SUBBAND_CONFIG_" + str(i)].split(":")
      self.subbands.append({ "cfreq": cfreq, "bw": bw, "nchan": nchan })

    freq_low  = float(self.subbands[0]["cfreq"])  - (float(self.subbands[0]["bw"]) / 2.0)
    freq_high  =float(self.subbands[-1]["cfreq"]) + (float(self.subbands[-1]["bw"]) / 2.0)
    self.out_freq = freq_low + ((freq_high - freq_low) / 2.0)

    return 0


class RepackBeamDaemon (RepackDaemon, BeamBased):

  def __init__ (self, name, id):
    RepackDaemon.__init__(self, name, str(id))
    BeamBased.__init__(self, str(id), self.cfg)

  def configure (self, become_daemon, dl, source, dest):
 
    self.log(1, "RepackBeamDaemon::configure()")
    Daemon.configure(self, become_daemon, dl, source, dest)
 
    self.processing_dir = self.cfg["CLIENT_FOLD_DIR"] + "/processing"
    self.finished_dir   = self.cfg["CLIENT_FOLD_DIR"] + "/finished"
    self.archived_dir   = self.cfg["CLIENT_FOLD_DIR"] + "/archived"

    bid = self.cfg["BEAM_" + str(self.beam_id)]

    self.beams.append(bid)
    self.results[bid] = {}
    self.results[bid]["valid"] = False

    # find the subbands for the specified beam that are processed by this script
    for isubband in range(int(self.cfg["NUM_SUBBAND"])):
      (cfreq , bw, nchan) = self.cfg["SUBBAND_CONFIG_" + str(isubband)].split(":")
      self.subbands.append({ "cfreq": cfreq, "bw": bw, "nchan": nchan })

    self.out_cfreq = cfreq
    self.log(1, "RepackBeamDaemon::configure done")

    return 0

###############################################################################

if __name__ == "__main__":

  if len(sys.argv) != 2:
    print "ERROR: 1 command line argument expected"
    sys.exit(1)

  # this should come from command line argument
  beam_id = sys.argv[1]

  script = []
  if int(beam_id) == -1:
    script = RepackServerDaemon ("repack")
  else:
    script = RepackBeamDaemon ("repack", beam_id)

  state = script.configure (DAEMONIZE, DL, "repack", "repack") 
  if state != 0:
    script.quit_event.set()
    sys.exit(state)

  script.log(1, "STARTING SCRIPT")

  try:

    reporting_thread = RepackReportingThread(script, beam_id)
    reporting_thread.start()

    script.main ()

    reporting_thread.join()

  except:

    script.log(-2, "exception caught: " + str(sys.exc_info()[0]))
    print '-'*60
    traceback.print_exc(file=sys.stdout)
    print '-'*60
    script.quit_event.set()

  script.log(1, "STOPPING SCRIPT")
  script.conclude()
  sys.exit(0)

