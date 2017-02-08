#!/usr/bin/env python

##############################################################################
#  
#     Copyright (C) 2015 by Andrew Jameson
#     Licensed under the Academic Free License version 2.1
# 
###############################################################################

import os, sys, socket, select, signal, traceback, time, threading, copy, string

from time import sleep

from spip.daemons.bases import BeamBased,ServerBased
from spip.daemons.daemon import Daemon
from spip.threads.reporting_thread import ReportingThread
from spip.utils import times,sockets
from spip.config import Config

DAEMONIZE = True
DL        = 1

class ResultsReportingThread(ReportingThread):

  def __init__ (self, script, id):
    host = sockets.getHostNameShort()
    port = int(script.cfg["STREAM_RESULTS_PORT"])
    if id >= 0:
      port += int(id)
    ReportingThread.__init__(self, script, host, port)

    with open (script.cfg["WEB_DIR"] + "/spip/images/blankimage.gif", mode='rb') as file:
      self.no_data = file.read()

  def parse_message (self, request):

    self.script.log (2, "ResultsReportingThread::parse_message: " + str(request))

    xml = ""
    req = request["results_request"]

    rs = self.script.results

    if req["type"] == "beam_list":
      xml = "<results_beam_list>"
      for beam in rs.keys():
        xml += "<beam>" + beam + "</beam>"
      xml += "</results_beam_list>"

    elif req["type"] == "utc_list":
      xml =  "<results_utc_list>"
      for utc in rs[req["beam"]].keys():
        xml += "<utc>" + utc + "</utc>"
      xml += "</results_utc_list>"

    elif req["type"] == "obs_list":

      index = int(req["index"])
      count = int(req["count"])

      self.script.results_lock.acquire()
      utcs = rs.keys()
      utcs.sort(reverse=True)

      if index > (len(utcs) - count):
        index = len (utcs) - count
      if index < 0:
        index = 0
      if count > len(utcs):
        count = len(utcs) 

      xml =  "<results_obs_list count='" + str(count) + "' total='" + str(self.script.source_number) + "'>"

      for i in range (index, index + count):
        utc = utcs[i]
        xml += "<utc_start utc='" + utc+ "'>"
        for source in rs[utc].keys():
          s = rs[utc][source]

          xml += "<source jname='" + source +"' index='" + str(i) + "'>"

          xml += "<beam>" + s["beam"] + "</beam>"
          xml += "<ra>" + s["ra"] + "</ra>"
          xml += "<dec>" + s["dec"] + "</dec>"
          xml += "<centre_frequency>" + s["centre_frequency"] + "</centre_frequency>"
          xml += "<bandwidth>" + s["bandwidth"] + "</bandwidth>"
          xml += "<nchannels>" + s["nchannels"] + "</nchannels>"
          xml += "<length>" + s["length"] + "</length>"
          xml += "<snr>" + s["snr"] + "</snr>"
          xml += "<subarray_id>" + s["subarray_id"] + "</subarray_id>"
          xml += "<project_id>" + s["project_id"] + "</project_id>"

          xml += "<plot type='flux_vs_phase'/>"
          xml += "<plot type='freq_vs_phase'/>"
          xml += "<plot type='time_vs_phase'/>"
          xml += "<plot type='bandpass'/>"

          xml += "</source>"

        xml += "</utc_start>"
      xml += "</results_obs_list>"
      self.script.results_lock.release()

      self.script.log (2, "ResultsReportingThread::parse_message: returning " + str(len(xml)) + " bytes of xml")
      return True, xml + "\r\n"

    elif req["type"] == "obs_info":
      return self.getObsInfo (req["utc_start"], req["source"])

    elif req["type"] == "obs_header":
      return self.getASCIIHeader (req["utc_start"], req["source"])

    elif req["type"] == "plot":

      # do a custom plot   
      if "xres" in req.keys() and "yres" in req.keys():
        rval, bin_data = self.script.custom_plot (req["beam"], req["utc_start"], req["source"], req["plot"], req["xres"], req["yres"])
        self.script.log (2, "ResultsReportingThread::parse_message: rval="+str(rval)+" bin_data="+str(bin_data))
        if rval < 0:
          bin_data = copy.deepcopy(self.no_data)
        return False, bin_data
      else:
        self.script.results_lock.acquire()
        self.script.log (2, "ResultsReportingThread::parse_message: beam=" + \
                            req["beam"] + " utc_start=" + req["utc_start"] + \
                            " source=" + req["source"] + " plot=" + req["plot"]) 

        bin_data = []
        if req["utc_start"] in self.script.results.keys() and \
           req["source"] in self.script.results[req["utc_start"]].keys() and \
           req["plot"] in self.script.results[req["utc_start"]][req["source"]].keys():
          bin_data = copy.deepcopy(self.script.results[req["utc_start"]][req["source"]][req["plot"]]["raw"])
        else:
          bin_data = copy.deepcopy(self.no_data)
        self.script.log (2, "ResultsReportingThread::parse_message: image len=" + str(len(bin_data)))
        self.script.results_lock.release()
        return False, bin_data


    else:
      xml += "<results_state>"
      xml += "<error>Invalid request</error>"
      xml += "</results_state>\r\n"

      return True, xml

  #############################################################################
  # return XML containing obs info
  def getObsInfo (self, utc_start, source):

    xml = "<results_obs_info>"
    xml +=  "<error>observation not found</error>"
    xml += "</results_obs_info>"

    if utc_start in  self.script.results.keys():
      if source in self.script.results[utc_start].keys():
        s = self.script.results[utc_start][source]
        xml = "<results_obs_info>"
        xml += "<utc_start>" + utc_start + "</utc_start>"
        xml += "<source>" + source +"</source>"
        xml += "<beam>" + s["beam"] + "</beam>"
        xml += "<ra>" + s["ra"] + "</ra>"
        xml += "<dec>" + s["dec"] + "</dec>"
        xml += "<centre_frequency>" + s["centre_frequency"] + "</centre_frequency>"
        xml += "<bandwidth>" + s["bandwidth"] + "</bandwidth>"
        xml += "<nchannels>" + s["nchannels"] + "</nchannels>"
        xml += "<length>" + s["length"] + "</length>"
        xml += "<snr>" + s["snr"] + "</snr>"
        xml += "<subarray_id>" + s["subarray_id"] + "</subarray_id>"
        xml += "<project_id>" + s["project_id"] + "</project_id>"

        xml += "<plot type='flux_vs_phase'/>"
        xml += "<plot type='freq_vs_phase'/>"
        xml += "<plot type='time_vs_phase'/>"
        xml += "<plot type='bandpass'/>"

        xml += "</results_obs_info>"

    return True, xml + "\r\n"

  #############################################################################
  # return XML containing obs header
  def getASCIIHeader(self, utc_start, source):

    xml = "<results_obs_header>"
    xml +=  "<error>observation not found</error>"
    xml += "</results_obs_header>"

    if utc_start in self.script.results.keys():
      if source in self.script.results[utc_start].keys():
        xml = "<results_obs_header>"
        xml += rs[utc_start][source]["header"] 
        xml += "</results_obs_header>"
    return True, xml 

class ResultsDaemon(Daemon):

  def __init__ (self, name, id):
    Daemon.__init__(self, name, str(id))

    self.valid_plots = ["freq_vs_phase", "flux_vs_phase", "time_vs_phase", "bandpass", "snr_vs_time"]
    self.beams = []
    self.subbands = []
    self.results = {}
    self.source_number = 0

  #################################################################
  # main
  #       id >= 0   process folded archives from a stream
  #       id == -1  process folded archives from all streams
  def main (self):

    self.log (2, "main: beams=" + str(self.beams))

    # finished observations stored in directory structure
    #  beam / utc_start / source

    if not os.path.exists(self.finished_dir):
      os.makedirs(self.finished_dir, 0755) 

    self.log (2, "main: stream_id=" + str(self.id))

    while (not self.quit_event.isSet()):

      self.log (2, "main: looking for new results")

      # check each beam for folded archives to process    
      for beam in self.beams:

        beam_dir = self.finished_dir + "/" + beam
        self.log (2, "main: beam=" + beam + " beam_dir=" + beam_dir)

        if not os.path.exists(beam_dir):
          os.makedirs(beam_dir, 0755)

        # get a list of all the recent observations
        cmd = "find " + beam_dir + " -mindepth 2 -maxdepth 2 -type d | sort"
        rval, observations = self.system (cmd, 3)

        # for each observation      
        for observation in observations:

          if self.quit_event.isSet():
            continue
   
          # strip prefix 
          observation = observation[(len(beam_dir)+1):]

          (utc_start, source) = observation.split("/")

          if source == "stats":
            continue
      
          self.results_lock.acquire()

          # check if summary data on this observation has already been collected
          if utc_start in self.results.keys():
            if source in self.results[utc_start].keys():
              self.results_lock.release()
              continue
            else:
              self.log (3, "results["+utc_start+"] existed, but no source yet")
              self.results[utc_start][source] = {}
          else:
            self.results[utc_start] = {}
            self.results[utc_start][source] = {}

          # collect summary data for this observation
          obs_dir = beam_dir + "/" + utc_start + "/" + source

          self.log (3, "main: collecting data for beam="+beam+" utc_start="+utc_start+" source="+source)

          (result, response) = self.collect_data (obs_dir, beam, utc_start, source)
          if not result == "ok":
            self.log (2, "main: removing beam="+beam+" utc_start="+utc_start+" source="+source)
            del self.results[utc_start][source]
            if len(self.results[utc_start].keys()) == 0:
              del self.results[utc_start]
          self.results_lock.release()

      self.log (2, "time.sleep(60)")
      to_sleep = 60
      while (to_sleep > 0 and not self.quit_event.isSet()):
        time.sleep(1)
        to_sleep -= 1

  # 
  # scrape the finished directory for meta-data about this observation
  #
  def collect_data(self, dir, beam, utc_start, source):

    data = self.results[utc_start][source]
 
    data["beam"] = beam 
    data["utc_start"] = utc_start
    data["source"] = source
    data["index"] = self.source_number
    self.source_number += 1

    # find the header filename
    cmd = "find " + dir + " -mindepth 1 -maxdepth 1 -type f -name 'obs.header*' | head -n 1"
    rval, lines = self.system (cmd, 3)
    if rval:
      return ("fail", data)
  
    header_file = lines[0]
    self.log (3, "collect_data: header_file=" + header_file)

    # read the contents of the header
    header = Config.readCFGFileIntoDict (header_file)

    data["centre_frequency"] = header["FREQ"]
    data["bandwidth"] = header["BW"]
    data["nchannels"] = header["NCHAN"]
    data["ra"] = header["RA"]
    data["dec"] = header["DEC"]
    data["mode"] = header["MODE"]
    data["project_id"] = header["PID"]
    data["subarray_id"] = "N/A"
    data["dir"] = dir
    data["length"] = "-1"
    data["snr"] = "-1"

    # convert entire header into XML
    keys = header.keys()
    keys.sort()
    for key in keys:
      data["header"] = "<" + key + ">" + header[key] + "</" + key + ">"

    psrplot_opts = "-c x:view='(0.0,1.0)' -c y:view='(0.0,1.0)' -g 160x120 -D -/png"

    time_sum_file = dir + "/time.sum"
    # find the path to the archives for plotting
    if os.path.exists(time_sum_file):
      data["time_sum"] = time_sum_file

      data["time_vs_phase"] = {}
      data["time_vs_phase"]["xres"] = 160
      data["time_vs_phase"]["yres"] = 120

      time_plot_file = dir + "/time.png"
      # if the plot does not exist, create it
      if not os.path.exists (time_plot_file):
        cmd = "psrplot -p time " + time_sum_file + " -jDp " + psrplot_opts
        rval, data["time_vs_phase"]["raw"] = self.system_raw (cmd, 3)
        if rval < 0:
          return (rval, "failed to generate time plot")
        fptr = open (time_plot_file, "wb")
        fptr.write(data["time_vs_phase"]["raw"])
        fptr.close()

      # read the created plot from the file system
      else:
        rval, data["time_vs_phase"]["raw"] = self.system_raw ("cat " + dir +"/time.png", 3)

    freq_sum_file = dir + "/freq.sum"
    if os.path.exists(freq_sum_file):
      data["freq_sum"] = freq_sum_file

      # generate the freq plot
      data["freq_vs_phase"] = {}
      data["freq_vs_phase"]["xres"] = 160
      data["freq_vs_phase"]["yres"] = 120

      freq_plot_file = dir + "/freq.png"
      if not os.path.exists (freq_plot_file):
        cmd = "psrplot -p freq " + freq_sum_file + " -jDp " + psrplot_opts
        rval, data["freq_vs_phase"]["raw"] = self.system_raw (cmd, 3)
        if rval < 0:
          return (rval, "failed to generate freq.png")
        fptr = open (freq_plot_file, "wb")
        fptr.write(data["freq_vs_phase"]["raw"])
        fptr.close()
      else:
        rval, data["freq_vs_phase"]["raw"] = self.system_raw ("cat " + dir +"/freq.png", 3)

      # generate the flux plot
      data["flux_vs_phase"] = {}
      data["flux_vs_phase"]["xres"] = 160
      data["flux_vs_phase"]["yres"] = 120

      flux_plot_file = dir + "/flux.png"
      if not os.path.exists (flux_plot_file):
        cmd = "psrplot -p flux " + freq_sum_file + " -jFDp " + psrplot_opts
        rval, data["flux_vs_phase"]["raw"] = self.system_raw (cmd, 3)
        if rval < 0:
          return (rval, "failed to create flux plot")
        fptr = open (flux_plot_file, "wb")
        fptr.write(data["flux_vs_phase"]["raw"])
        fptr.close()
      else:
        rval, data["flux_vs_phase"]["raw"] = self.system_raw ("cat " + dir +"/flux.png", 3)

    band_file = dir + "/band.last"
    if os.path.exists(band_file):
      data["band_last"] = band_file

      data["bandpass"] = {}
      data["bandpass"]["xres"] = 160
      data["bandpass"]["yres"] = 120
      band_plot_file = dir + "/band.png"
      if not os.path.exists (band_plot_file):
        cmd = "psrplot -p b " + band_file + " -x -lpol=0,1 -N2,1 " + psrplot_opts
        rval, data["bandpass"]["raw"] = self.system_raw (cmd, 3)
        if rval < 0:
          return (rval, "failed to create band plot")
        fptr = open (band_plot_file, "wb")
        fptr.write(data["bandpass"]["raw"])
        fptr.close()
      else:
        rval, data["bandpass"]["raw"] = self.system_raw ("cat " + band_plot_file, 3)

    # find the resultsfilename
    results_file = dir + "/obs.results"
    if os.path.exists(results_file):
      self.log (3, "collect_data: results_file=" + results_file)
      results = Config.readCFGFileIntoDict (results_file)
      data["snr"] = results["snr"]
      data["length"] = results["length"]
    else:
      if os.path.exists(freq_sum_file):
        cmd = "psrstat -jFDp -c snr " + freq_sum_file + " | awk -F= '{printf(\"%f\",$2)}'"
        rval, lines = self.system (cmd, 3)
        if rval < 0:
          return (rval, "failed to extract snr from freq.sum")
        data["snr"] = lines[0]

      # determine the length of the observation
      if os.path.exists(time_sum_file):
        cmd = "psrstat -c length " + time_sum_file + " | awk -F= '{printf(\"%f\",$2)}'"
        rval, lines = self.system (cmd, 3)
        if rval < 0:
          return (rval, "failed to extract length from time.sum")
        data["length"] = lines[0]

      # write these values to the sum file
      fptr = open (results_file, "w")
      fptr.write("snr\t" + data["snr"] + "\n")
      fptr.write("length\t" + data["length"] + "\n")
      fptr.close()

    return ("ok", "collected")

  #############################################################################
  # produce a PSR plot with the specified dimensions
  def custom_plot (self, beam, utc_start, source, plot, xres, yres):
    bin_data = []
    rval = -1
    self.results_lock.acquire()
    try:
      opts = " -D -/png"
      opts += " -g " + str(xres) + "x" + str(yres)
      opts += " -c above:c=''"
      if int(xres) < 200:
        opts += " -c x:view='(0.0,1.0)' -c y:view='(0.0,1.0)'"
      if plot == "freq_vs_phase":
        cmd = "psrplot -p freq -jDp " + self.results[utc_start][source]["freq_sum"] + opts
      elif plot == "flux_vs_phase":
        cmd = "psrplot -p flux -jFDp " + self.results[utc_start][source]["freq_sum"] + opts
      elif plot == "time_vs_phase":
        cmd = "psrplot -p time -jDp " + self.results[utc_start][source]["time_sum"] + opts
      elif plot == "bandpass":
        cmd = "psrplot -jD -p b -x -lpol=0,1 -N2,1 " + self.results[utc_start][source]["band_last"] + opts
      rval, bin_data = self.system_raw (cmd, 3)

    except KeyError as e:
      self.log("freq_plot: results["+utc_start+"]["+source+"][*] did not exist")

    self.results_lock.release()
    return rval, bin_data

class ResultsServerDaemon (ResultsDaemon, ServerBased):

  def __init__ (self, name):
    ResultsDaemon.__init__(self,name, "-1")
    ServerBased.__init__(self, self.cfg)

  def configure (self,become_daemon, dl, source, dest):

    Daemon.configure (self, become_daemon, dl, source, dest)

    self.finished_dir   = self.cfg["SERVER_FOLD_DIR"] + "/finished"

    for i in range(int(self.cfg["NUM_BEAM"])):
      bid = self.cfg["BEAM_" + str(i)]
      self.beams.append(bid)
      self.results_lock = threading.Lock()
      self.results_cond = threading.Condition(self.results_lock)

    return 0

  def conclude (self):
    for i in range(int(self.cfg["NUM_BEAM"])):
      bid = self.cfg["BEAM_" + str(i)]
    self.results_lock.release()

    ResultsDaemon.conclude()


class ResultsBeamDaemon (ResultsDaemon, BeamBased):

  def __init__ (self, name, id):
    ResultsDaemon.__init__(self, name, str(id))
    BeamBased.__init__(self, str(id), self.cfg)

  def configure (self, become_daemon, dl, source, dest):
 
    self.log(1, "ResultsBeamDaemon::configure()")
    Daemon.configure(self, become_daemon, dl, source, dest)
 
    self.finished_dir = self.cfg["CLIENT_FOLD_DIR"] + "/finished"

    bid = self.cfg["BEAM_" + str(self.beam_id)]

    self.beams.append(bid)
    self.results_lock = threading.Lock()
    self.results_cond = threading.Condition(self.results_lock)

    self.log(1, "ResultsBeamDaemon::configure done")

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
    script = ResultsServerDaemon ("spip_results")
  else:
    script = ResultsBeamDaemon ("spip_results", beam_id)

  state = script.configure (DAEMONIZE, DL, "results", "results") 
  if state != 0:
    script.quit_event.set()
    sys.exit(state)

  script.log(1, "STARTING SCRIPT")

  try:

    reporting_thread = ResultsReportingThread(script, beam_id)
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

