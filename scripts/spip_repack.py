#!/usr/bin/env python

##############################################################################
#  
#     Copyright (C) 2015 by Andrew Jameson
#     Licensed under the Academic Free License version 2.1
# 
###############################################################################

import os, sys, socket, select, signal, traceback, time, threading, copy, string, shutil

from time import sleep

from spip.daemons.bases import BeamBased,ServerBased
from spip.daemons.daemon import Daemon
from spip.threads.reporting_thread import ReportingThread
from spip.utils import times,sockets
from spip.config import Config
from spip.plotting import SNRPlot

DAEMONIZE = True
DL        = 1

class RepackReportingThread(ReportingThread):

  def __init__ (self, script, id):
    host = sockets.getHostNameShort()
    port = int(script.cfg["STREAM_REPACK_PORT"])
    if id >= 0:
      port += int(id)
    ReportingThread.__init__(self, script, host, port)

    with open (script.cfg["WEB_DIR"] + "/spip/images/blankimage.gif", mode='rb') as file:
      self.no_data = file.read()

  def parse_message (self, request):

    self.script.log (2, "RepackReportingThread::parse_message: " + str(request))

    xml = ""
    req = request["repack_request"]

    if req["type"] == "state":

      self.script.log (3, "RepackReportingThread::parse_message: preparing state response")
      xml = "<repack_state>"

      for beam in self.script.beams:

        self.script.log (3, "RepackReportingThread::parse_message: preparing state for beam: " + str(beam))

        self.script.results[beam]["lock"].acquire()
        xml += "<beam name='" + str(beam) + "' active='" + str(self.script.results[beam]["valid"]) + "'>"

        self.script.log (3, "RepackReportingThread::parse_message: keys="+str(self.script.results[beam].keys()))

        if self.script.results[beam]["valid"]:

          self.script.log (3, "RepackReportingThread::parse_message: beam " + str(beam) + " is valid!")
          xml += "<source>"
          xml += "<name epoch='J2000'>" + self.script.results[beam]["source"] + "</name>"
          xml += "</source>"

          xml += "<observation>"
          xml += "<start units='datetime'>" + self.script.results[beam]["utc_start"] + "</start>"
          xml += "<integrated units='seconds'>" + self.script.results[beam]["length"] + "</integrated>"
          xml += "<snr>" + self.script.results[beam]["snr"] + "</snr>"
          xml += "</observation>"

          xml += "<plot type='flux_vs_phase' timestamp='" + self.script.results[beam]["timestamp"] + "'/>"
          xml += "<plot type='freq_vs_phase' timestamp='" + self.script.results[beam]["timestamp"] + "'/>"
          xml += "<plot type='time_vs_phase' timestamp='" + self.script.results[beam]["timestamp"] + "'/>"
          xml += "<plot type='bandpass' timestamp='" + self.script.results[beam]["timestamp"] + "'/>"
          xml += "<plot type='snr_vs_time' timestamp='" + self.script.results[beam]["timestamp"] + "'/>"

        xml += "</beam>"

        self.script.results[beam]["lock"].release()

      xml += "</repack_state>"
      self.script.log (2, "RepackReportingThread::parse_message: returning " + str(xml))
    
      return True, xml + "\r\n"

    elif req["type"] == "plot":
     
      if req["plot"] in self.script.valid_plots:

        self.script.results[req["beam"]]["lock"].acquire()
        self.script.log (2, "RepackReportingThread::parse_message: beam=" + \
                          req["beam"] + " plot=" + req["plot"]) 

        if self.script.results[req["beam"]]["valid"]:
          bin_data = copy.deepcopy(self.script.results[req["beam"]][req["plot"]])
          self.script.log (2, "RepackReportingThread::parse_message: beam=" + req["beam"] + " valid, image len=" + str(len(bin_data)))
          self.script.results[req["beam"]]["lock"].release()
          return False, bin_data
        else:
          self.script.log (2, "RepackReportingThread::parse_message beam was not valid")

          self.script.results[req["beam"]]["lock"].release()
          # still return if the timestamp is recent
          return False, self.no_data

      xml += "<repack_state>"
      xml += "<error>Invalid request</error>"
      xml += "</repack_state>\r\n"

      return True, xml

class RepackDaemon(Daemon):

  def __init__ (self, name, id):
    Daemon.__init__(self, name, str(id))

    self.valid_plots = ["freq_vs_phase", "flux_vs_phase", "time_vs_phase", "bandpass", "snr_vs_time"]
    self.beams = []
    self.subbands = []
    self.results = {}
    self.snr_history = {}

    self.snr_plot = SNRPlot()

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
    # out_cfreq = 0

    if not os.path.exists(self.processing_dir):
      os.makedirs(self.processing_dir, 0755) 
    if not os.path.exists(self.finished_dir):
      os.makedirs(self.finished_dir, 0755) 
    if not os.path.exists(self.archived_dir):
      os.makedirs(self.archived_dir, 0755) 

    self.log (2, "main: stream_id=" + str(self.id))

    while (not self.quit_event.isSet()):

      processed_this_loop = 0

      # check each beam for folded archives to process    
      for beam in self.beams:

        beam_dir = self.processing_dir + "/" + beam
        self.log (3, "main: beam=" + beam + " beam_dir=" + beam_dir)

        if not os.path.exists(beam_dir):
          os.makedirs(beam_dir, 0755)

        # get a list of all the recent observations
        cmd = "find " + beam_dir + " -mindepth 2 -maxdepth 2 -type d"
        rval, observations = self.system (cmd, 3)

        # for each observation      
        for observation in observations:
   
          # strip prefix 
          observation = observation[(len(beam_dir)+1):]

          (utc, source) = observation.split("/")

          if source == "stats":
            continue

          obs_dir = beam_dir + "/" + observation
          out_dir = self.archived_dir + "/" + beam + "/" + utc + "/" + source + "/" + str(self.out_cfreq)

          if not os.path.exists(out_dir):
            os.makedirs(out_dir, 0755)

          # if we have only 1 sub-band, then files can be processed immediately
          archives = {}
          for subband in self.subbands:
            self.log (3, "processing subband=" + str(subband))
            
            cmd = "find " + obs_dir + "/" + subband["cfreq"] + " -mindepth 1 -maxdepth 1 " + \
                  "-type f -name '" + archives_glob + "' -printf '%f\\n'"
            rval, files = self.system (cmd, 3)

            for file in files:
              if not file in archives:
                archives[file] = 0
              archives[file] += 1

          # if a file meets the subband count it is ripe for processing
          files = archives.keys()
          files.sort()

          for file in files:

            processed_this_loop += 1

            self.log (1, observation + ": processing " + file)

            if archives[file] == len(self.subbands):
              if len(self.subbands) > 1:
                self.log (2, "main: process_subband()")
                (rval, response) = self.process_subband (obs_dir, out_dir, source, file)
                if rval:
                  self.log (-1, "failed to process sub-bands for " + file + ": " + response)
              else:
                input_file  = obs_dir  + "/" + self.subbands[0]["cfreq"] + "/" + file
                self.log (2, "main: process_archive() "+ input_file)
                (rval, response) = self.process_archive (obs_dir, input_file, out_dir, source)
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
          any_failed = False

          # perhaps a file was produced whilst the previous list was being processed,
          # do another pass
          if len(files) > 0:
            all_finished = False

          for subband in self.subbands:
            filename = obs_dir + "/" + subband["cfreq"] + "/obs.finished"
            if os.path.exists(filename):
              if os.path.getmtime(filename) + 10 > time.time():
                all_finished = False
            else:
              all_finished = False
            filename = obs_dir + "/" + subband["cfreq"] + "/obs.failed"
            if os.path.exists(filename):
              any_failed = True
         
          # the observation has failed, cleanup
          if any_failed:
            self.log (1, observation + ": processing -> failed")
            all_finished = False

            fail_parent_dir = self.failed_dir + "/" + beam + "/" + utc
            if not os.path.exists(fail_parent_dir):
              os.makedirs(fail_parent_dir, 0755)
            fail_dir = self.failed_dir + "/" + beam + "/" + utc + "/" + source
            self.log (2, "main: fail_observation("+obs_dir+")")
            (rval, response) = self.fail_observation (beam, obs_dir, fail_dir, out_dir)
            if rval:
              self.log (-1, "failed to finalise observation: " + response)

          # The observation has finished, cleanup
          if all_finished: 
            self.log (1, observation + ": processing -> finished")

            fin_parent_dir = self.finished_dir + "/" + beam + "/" + utc
            if not os.path.exists(fin_parent_dir):
              os.makedirs(fin_parent_dir, 0755)

            fin_dir = self.finished_dir + "/" + beam + "/" + utc + "/" + source
            self.log (2, "main: finalise_observation("+obs_dir+")")
            (rval, response) = self.finalise_observation (beam, obs_dir, fin_dir, out_dir)
            if rval:
              self.log (-1, "failed to finalise observation: " + response)
            else:

              # merge the headers from each sub-band
              header = Config.readCFGFileIntoDict (fin_dir + "/" + self.subbands[0]["cfreq"] + "/obs.header")
              for i in range(1,len(self.subbands)):
                header_sub = Config.readCFGFileIntoDict (fin_dir + "/" + self.subbands[i]["cfreq"] + "/obs.header")
                header = Config.mergerHeaderFreq (header, header_sub)
                os.remove (fin_dir + "/" + self.subbands[i]["cfreq"] + "/obs.header")
                os.remove (fin_dir + "/" + self.subbands[i]["cfreq"] + "/obs.finished")
                os.removedirs (fin_dir + "/" + self.subbands[i]["cfreq"])
              os.remove (fin_dir + "/" + self.subbands[0]["cfreq"] + "/obs.header")
              os.remove (fin_dir + "/" + self.subbands[0]["cfreq"] + "/obs.finished")
              os.removedirs (fin_dir + "/" + self.subbands[0]["cfreq"])

              Config.writeDictToCFGFile (header, fin_dir + "/" + "obs.header")
              shutil.copyfile (fin_dir + "/obs.header", out_dir + "/obs.header")


      if processed_this_loop == 0:
        self.log (3, "time.sleep(1)")
        time.sleep(1)

  def

  # 
  # patch missing information into the PSRFITS header 
  #
  def patch_psrfits_header (self, input_dir, input_file):

    header_file = input_dir + "/obs.header"
    self.log(3, "patch_psrfits_header: header_file="+header_file)

    header = Config.readCFGFileIntoDict (input_dir + "/obs.header")

    new = {}
    new["obs:observer"] = header["OBSERVER"] 
    new["obs:projid"]   = header["PID"]

    # constants that currently do not flow through CAM
    new["be:nrcvr"]     = "2"

    # need to know what these mean!
    new["be:phase"]     = "+1"    # Phase convention of backend
    new["be:tcycle"]    = "8"     # Correlator cycle time
    new["be:dcc"]       = "0"     # Downconversion conjugation corrected
    new["sub:nsblk"]    = "1"     # Samples/row (SEARCH mode, else 1)
  
    # this needs to come from CAM, hack for now
    new["ext:trk_mode"] = "TRACK" # Tracking mode
    new["ext:bpa"]      = "0" # Beam position angle [?]
    new["ext:bmaj"]     = "0" # Beam major axis [degrees]
    new["ext:bmin"]     = "0" # Beam minor axis [degrees]

    new["ext:obsfreq"]  = header["FREQ"]
    new["ext:obsbw"]    = header["BW"]
    new["ext:obsnchan"] = header["NCHAN"]

    new["ext:stp_crd1"] = header["RA"]
    new["ext:stp_crd2"] = header["DEC"]
    new["ext:stt_date"] = header["UTC_START"][0:10]
    new["ext:stt_time"] = header["UTC_START"][11:19]

    # create the psredit command necessary to apply "new"
    cmd = "psredit -m -c " + ",".join(['%s=%s' % (key, value) for (key, value) in new.items()]) + " " + input_file
    rval, lines = self.system(cmd, 2)
    if rval:
      return rval, lines[0]
    return 0, ""

  #
  # process and file in the directory, adding file to 
  #
  def process_archive (self, in_dir, input_file, out_dir, source):

    (rval, response) = self.acquire_obs_header (in_dir)
    if rval:
      return (-1, "process_archive: could not acquire obs.header")

    self.log (2, "process_archive() input_file=" + input_file)

    freq_file   = in_dir + "/freq.sum"
    time_file   = in_dir + "/time.sum"
    band_file   = in_dir + "/band.last"

    # convert the timer archive file to psrfits in the output directory
    cmd = "psrconv " + input_file + " -O " + out_dir
    rval, lines = self.system (cmd, 2)
    if rval:
      return (rval, "failed to copy processed file to archived dir")
    psrfits_file = out_dir + "/" + os.path.basename (input_file)
    # psrfits_file = string.replace(psrfits_file, ".ar", ".rf")

    self.log (2, "process_archive() psrfits_file=" + psrfits_file)

    # update the header parameters in the psrfits file
    (rval, message) = self.patch_psrfits_header (in_dir, psrfits_file)
    if rval:
      return (rval, "failed to convert psrfits file")

    # bscrunch to 4 bins for the bandpass plot
    bscr_factor = int(self.total_channels / 4)
    cmd = "pam -b " + str(bscr_factor) + " -e bscr " + input_file
    rval, lines = self.system (cmd, 2)
    if rval:
      return (rval, "could not bscrunch to 4 bins")

    # copy/overwrite the bandpass file
    input_band_file = string.replace(input_file, ".ar", ".bscr")
    if os.path.exists (band_file):
      os.remove (band_file)
    cmd = "mv " + input_band_file + " " + band_file
    rval, lines = self.system (cmd, 2)
    if rval:
      return (rval, "failed to copy recent band file")

    # auto-zap bad channels
    cmd = "zap.psh -m " + input_file
    rval, lines = self.system (cmd, 2)
    if rval:
      return (rval, "failed to zap known bad channels")

    # pscrunch and fscrunch to 512 channels for the Fsum
    fscr_factor = int(self.total_channels / 512)
    cmd = "pam -p -f " + str(fscr_factor) + " -m " + input_file
    rval, lines = self.system (cmd, 2)
    if rval:
      return (rval, "could not fscrunch to 512 channels")

    # add the archive to the freq sum, tscrunching it
    if not os.path.exists(freq_file):
      cmd = "cp " + input_file + " " + freq_file
      rval, lines = self.system (cmd, 2)
      if rval:
        return (rval, "failed to copy archive to freq.sum")
    else:
      cmd = "psradd -T -o " + freq_file + " " + freq_file + " " + input_file
      rval, lines = self.system (cmd, 2)
      if rval:
        return (rval, "failed add archive to freq.sum")

    # fscrunch the archive 
    cmd = "pam -m -F " + input_file
    rval, lines = self.system (cmd, 2)
    if rval:
      return (rval, "failed add Fscrunch archive")

    # add it to the time sum
    if not os.path.exists(time_file):
      try:
        os.rename (input_file, time_file)
      except OSError, e:
        return (-1, "failed rename Fscrunched archive to time.sum: " + str(e))
    else:
      cmd = "psradd -o " + time_file + " " + time_file + " " + input_file
      rval, lines = self.system (cmd, 2)
      if rval:
        return (rval, "failed add Fscrunched archive to time.sum")
      try:
        os.remove (input_file)
      except OSError, e:
        return (-1, "failed remove Fscrunched archive")

    return (0, "")

  def acquire_obs_header (self, in_dir):
    if not os.path.exists (in_dir + "/obs.header"):
      cmd = "find " + in_dir + " -mindepth 2 -maxdepth 2 -type f -name 'obs.header'"
      rval, header_files  = self.system (cmd, 3)
      if rval or len(header_files) == 0:
        return (-1, "acquire_obs_header: could not find obs.header files")

      cmd = "cp " + header_files[0] + " " + in_dir +"/obs.header"
      rval, header_files  = self.system (cmd, 3)
      if rval:
        return (-1, "acquire_obs_header: could not copy obs.header file")
    return (0, "")

  #
  # process all sub-bands for the same archive
  #
  def process_subband (self, in_dir, out_dir, source, file):

    interim_file = "/dev/shm/" + file
    input_files = in_dir + "/*/" + file

    cmd = "psradd -R -o " + interim_file + " " + input_files
    rval, observations = self.system (cmd, 3)
    if rval:
      return (rval, "failed to add sub-band archives to interim file")

    (rval, response) = self.process_archive (in_dir, interim_file, out_dir, source)
    if rval:
      return (rval, "process_archive failed: " + response)
    
    # remove in the input sub-banded files
    cmd = "rm -f " + input_files
    rval, lines = self.system (cmd, 3)
    if rval:
      return (rval, "failed to delete input files")

    return (0, "")

  def process_observation (self, beam, utc, source, in_dir):

    freq_file   = in_dir + "/freq.sum"
    time_file   = in_dir + "/time.sum"
    band_file   = in_dir + "/band.last"

    timestamp = times.getCurrentTime() 

    cmd = "psrplot -p freq " + freq_file + " -jDp -j 'F 512' -D -/png"
    rval, freq_raw = self.system_raw (cmd, 3)
    if rval < 0:
      return (rval, "failed to create freq plot")

    cmd = "psrplot -p time " + time_file + " -jDp -D -/png"
    rval, time_raw = self.system_raw (cmd, 3)
    if rval < 0:
      return (rval, "failed to create time plot")

    cmd = "psrplot -p flux -jFD " + freq_file + " -jp -D -/png"
    rval, flux_raw = self.system_raw (cmd, 3)
    if rval < 0:
      return (rval, "failed to create time plot")

    cmd = "psrplot -pb -x -lpol=0,1 -N2,1 -c above:c= " + band_file + " -D -/png"
    rval, bandpass_raw = self.system_raw (cmd, 3)
    if rval < 0:
      return (rval, "failed to create time plot")

    cmd = "psrstat -jFDp -c snr " + freq_file + " | awk -F= '{printf(\"%f\",$2)}'"
    rval, lines = self.system (cmd, 3)
    if rval < 0:
      return (rval, "failed to extract snr from freq.sum")
    snr = lines[0]

    cmd = "psrstat -c length " + time_file + " | awk -F= '{printf(\"%f\",$2)}'"
    rval, lines = self.system (cmd, 3)
    if rval < 0:
      return (rval, "failed to extract time from time.sum")
    length = lines[0]

    self.results[beam]["lock"].acquire() 

    self.results[beam]["utc_start"] = utc
    self.results[beam]["source"] = source
    self.results[beam]["freq_vs_phase"] = freq_raw
    self.results[beam]["flux_vs_phase"] = flux_raw
    self.results[beam]["time_vs_phase"] = time_raw
    self.results[beam]["bandpass"] = bandpass_raw 
    self.results[beam]["timestamp"] = timestamp
    self.results[beam]["snr"] = snr
    self.results[beam]["length"] = length
    self.results[beam]["valid"] = True
    
    t1 = int(times.convertLocalToUnixTime(timestamp))
    t2 = int(times.convertUTCToUnixTime(utc))
    delta_time = t1 - t2
    self.snr_history[beam]["times"].append(delta_time)
    self.snr_history[beam]["snrs"].append(snr)

    self.snr_plot.configure()
    self.snr_plot.plot (240, 180, False, self.snr_history[beam]["times"], self.snr_history[beam]["snrs"])
    self.log (2, "process_observation: snr_plot len=" + str(len(self.snr_plot.getRawImage())))
    self.results[beam]["snr_vs_time"] = self.snr_plot.getRawImage()

    self.results[beam]["lock"].release() 

    return (0, "")

  def fail_observation (self, beam, obs_dir, fail_dir, arch_dir):

    self.end_observation (beam, obs_dir, fail_dir, arch_dir)

    # touch obs.failed file in the archival directory
    cmd = "touch " + arch_dir + "/obs.failed"
    rval, lines = self.system (cmd, 3)

    # simply move the observation to the failed directory
    try:
      os.rename (obs_dir, fail_dir)
    except OSError, e:
      return (1, "failed to rename obs_dir to fail_dir")

    # delete the parent directory of obs_dir
    parent_dir = os.path.dirname (obs_dir)
    os.removedirs(parent_dir)
    return (0, "")


  def finalise_observation (self, beam, obs_dir, fin_dir, arch_dir):

    self.end_observation (beam, obs_dir, fin_dir, arch_dir)

    # touch and obs.finished file in the archival directory
    cmd = "touch " + arch_dir + "/obs.finished"
    rval, lines = self.system (cmd, 3)

    # simply move the observation to the finished directory
    try:
      os.rename (obs_dir, fin_dir)
    except OSError, e:
      return (1, "failed to rename obs_dir to fin_dir")

    # delete the parent directory of obs_dir
    parent_dir = os.path.dirname (obs_dir)
    os.removedirs(parent_dir)
    return (0, "")


  def end_observation (self, beam, obs_dir, fin_dir, arch_dir)    :

    # write the most recent images disk for long term storage
    timestamp = times.getCurrentTime()
    
    self.results[beam]["lock"].acquire()

    self.log (2, "end_observation: beam=" + beam + " timestamp=" + \
              timestamp + " valid=" + str(self.results[beam]["valid"]))

    if (self.results[beam]["valid"]):
    
      flux_vs_phase = obs_dir + "/" + timestamp + ".flux.png"
      freq_vs_phase = obs_dir + "/" + timestamp + ".freq.png"
      time_vs_phase = obs_dir + "/" + timestamp + ".time.png"
      bandpass = obs_dir + "/" + timestamp + ".band.png"

      fptr = open (flux_vs_phase, "wb")
      fptr.write(self.results[beam]["flux_vs_phase"])
      fptr.close()

      fptr = open (freq_vs_phase, "wb")
      fptr.write(self.results[beam]["freq_vs_phase"])
      fptr.close()

      fptr = open (time_vs_phase, "wb")
      fptr.write(self.results[beam]["time_vs_phase"])
      fptr.close()

      fptr = open (bandpass, "wb")
      fptr.write(self.results[beam]["bandpass"])
      fptr.close()

      self.snr_history[beam]["times"] = []
      self.snr_history[beam]["snrs"] = []

      # indicate that the beam is no longer valid now that the 
      # observation has finished
      self.results[beam]["valid"] = False

    self.results[beam]["lock"].release()


class RepackServerDaemon (RepackDaemon, ServerBased):

  def __init__ (self, name):
    RepackDaemon.__init__(self,name, "-1")
    ServerBased.__init__(self, self.cfg)

  def configure (self,become_daemon, dl, source, dest):

    Daemon.configure (self, become_daemon, dl, source, dest)

    self.processing_dir = self.cfg["SERVER_FOLD_DIR"] + "/processing"
    self.finished_dir   = self.cfg["SERVER_FOLD_DIR"] + "/finished"
    self.archived_dir   = self.cfg["SERVER_FOLD_DIR"] + "/archived"
    self.failed_dir     = self.cfg["SERVER_FOLD_DIR"] + "/failed"

    for i in range(int(self.cfg["NUM_BEAM"])):
      bid = self.cfg["BEAM_" + str(i)]
      self.beams.append(bid)
      self.results[bid] = {}
      self.results[bid]["valid"] = False
      self.results[bid]["lock"] = threading.Lock()
      self.results[bid]["cond"] = threading.Condition(self.results[bid]["lock"])

      self.snr_history[bid] = {}
      self.snr_history[bid]["times"] = []
      self.snr_history[bid]["snrs"] = []

    self.total_channels = 0
    for i in range(int(self.cfg["NUM_SUBBAND"])):
      (cfreq , bw, nchan) = self.cfg["SUBBAND_CONFIG_" + str(i)].split(":")
      self.subbands.append({ "cfreq": cfreq, "bw": bw, "nchan": nchan })
      self.total_channels += int(nchan)

    freq_low  = float(self.subbands[0]["cfreq"])  - (float(self.subbands[0]["bw"]) / 2.0)
    freq_high = float(self.subbands[-1]["cfreq"]) + (float(self.subbands[-1]["bw"]) / 2.0)
    self.out_freq = freq_low + ((freq_high - freq_low) / 2.0)

    return 0

  def conclude (self):
    for i in range(int(self.cfg["NUM_BEAM"])):
      bid = self.cfg["BEAM_" + str(i)]
    self.results[bid]["lock"].release()

    RepackDaemon.conclude()



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
    self.failed_dir     = self.cfg["CLIENT_FOLD_DIR"] + "/failed"

    bid = self.cfg["BEAM_" + str(self.beam_id)]

    self.beams.append(bid)
    self.results[bid] = {}
    self.results[bid]["valid"] = False
    self.results[bid]["lock"] = threading.Lock()
    self.results[bid]["cond"] = threading.Condition(self.results[bid]["lock"])

    self.snr_history[bid] = {}
    self.snr_history[bid]["times"] = []
    self.snr_history[bid]["snrs"] = []

    # find the subbands for the specified beam that are processed by this script
    self.total_channels = 0
    for isubband in range(int(self.cfg["NUM_SUBBAND"])):
      (cfreq , bw, nchan) = self.cfg["SUBBAND_CONFIG_" + str(isubband)].split(":")
      self.subbands.append({ "cfreq": cfreq, "bw": bw, "nchan": nchan })
      self.total_channels += int(nchan)

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
    script = RepackServerDaemon ("spip_repack")
  else:
    script = RepackBeamDaemon ("spip_repack", beam_id)

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
    formatted_lines = traceback.format_exc().splitlines()
    script.log(0, '-'*60)
    for line in formatted_lines:
      script.log(0, line)
    script.log(0, '-'*60)

    print '-'*60
    traceback.print_exc(file=sys.stdout)
    print '-'*60
    script.quit_event.set()

  script.log(1, "STOPPING SCRIPT")
  script.conclude()
  sys.exit(0)

