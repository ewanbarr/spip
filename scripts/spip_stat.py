#!/usr/bin/env python

##############################################################################
#  
#     Copyright (C) 2015 by Andrew Jameson
#     Licensed under the Academic Free License version 2.1
# 
###############################################################################

import os, sys, socket, select, signal, traceback, time, threading, copy
import numpy as np

from spip.daemons.bases import StreamBased
from spip.daemons.daemon import Daemon
from spip.threads.reporting_thread import ReportingThread
from spip.log_socket import LogSocket
from spip.utils import times,sockets
from spip.utils.core import system_piped
from spip.plotting import HistogramPlot,FreqTimePlot

from spip_smrb import SMRBDaemon

DAEMONIZE = True
DL        = 1

class StatReportingThread(ReportingThread):

  def __init__ (self, script, id):
    host = sockets.getHostNameShort()
    port = int(script.cfg["STREAM_STAT_PORT"])
    if id >= 0:
      port += int(id)
    ReportingThread.__init__(self, script, host, port)

    with open (script.cfg["WEB_DIR"] + "/spip/images/blankimage.gif", mode='rb') as file:
      self.no_data = file.read()

  def parse_message (self, request):
    self.script.log (2, "StatReportingThread::parse_message: " + str(request))

    xml = ""
    req = request["stat_request"]

    if req["type"] == "state":

      self.script.log (3, "StatReportingThread::parse_message: preparing state response")
      xml = "<stat_state>"

      self.script.results["lock"].acquire()
      xml += "<stream id='" + str(self.script.id) + "' beam_name='" + self.script.beam + "' active='" + str(self.script.results["valid"]) + "'>"

      self.script.log (3, "StatReportingThread::parse_message: keys="+str(self.script.results.keys()))

      if self.script.results["valid"]:

        self.script.log (3, "StatReportingThread::parse_message: stream is valid!")

        xml += "<polarisation name='0'>"
        xml += "<dimension name='real'>"
        xml += "<histogram_mean>" + str(self.script.results["hg_mean_0_re"]) + "</histogram_mean>"
        xml += "<histogram_stddev>" + str(self.script.results["hg_stddev_0_re"]) + "</histogram_stddev>"
        xml += "</dimension>"
        xml += "<dimension name='imag'>"
        xml += "<histogram_mean>" + str(self.script.results["hg_mean_0_im"]) + "</histogram_mean>"
        xml += "<histogram_stddev>" + str(self.script.results["hg_stddev_0_im"]) + "</histogram_stddev>"
        xml += "</dimension>"
        xml += "<plot type='histogram' timestamp='" + str(self.script.results["timestamp"]) + "'/>"
        xml += "<plot type='freq_vs_time' timestamp='" + str(self.script.results["timestamp"]) + "'/>"
        xml += "</polarisation>"

        xml += "<polarisation name='1'>"
        xml += "<dimension name='real'>"
        xml += "<histogram_mean>" + str(self.script.results["hg_mean_1_re"]) + "</histogram_mean>"
        xml += "<histogram_stddev>" + str(self.script.results["hg_stddev_1_re"]) + "</histogram_stddev>"
        xml += "</dimension>"
        xml += "<dimension name='imag'>"
        xml += "<histogram_mean>" + str(self.script.results["hg_mean_1_im"]) + "</histogram_mean>"
        xml += "<histogram_stddev>" + str(self.script.results["hg_stddev_1_im"]) + "</histogram_stddev>"
        xml += "</dimension>"
        xml += "<plot type='histogram' timestamp='" + str(self.script.results["timestamp"]) + "'/>"
        xml += "<plot type='freq_vs_time' timestamp='" + str(self.script.results["timestamp"]) + "'/>"
        xml += "</polarisation>"

      xml += "</stream>"

      self.script.results["lock"].release()

      xml += "</stat_state>"
      self.script.log (2, "StatReportingThread::parse_message: returning " + str(xml))

      return True, xml + "\r\n"

    elif req["type"] == "plot":
     
      if req["plot"] in self.script.valid_plots:

        self.script.results["lock"].acquire()
        self.script.log (2, "StatReportingThread::parse_message: " + \
                         " plot=" + req["plot"] + " pol=" + req["pol"]) 

        if self.script.results["valid"]:
          plot = req["plot"] + "_" + req["pol"]
          if plot in self.script.results.keys():
            if len (self.script.results[plot]) > 64:
              bin_data = copy.deepcopy(self.script.results[plot])
              self.script.log (2, "StatReportingThread::parse_message: valid, image len=" + str(len(bin_data)))
              self.script.results["lock"].release()
              return False, bin_data
            else:
              self.script.log (1, "StatReportingThread::parse_message len <= 64")
          else:
            self.script.log (1, "StatReportingThread::parse_message plot ["+plot+"] not in keys [" + str(self.script.results.keys()))
        else:
          self.script.log (1, "StatReportingThread::parse_message results not valid")

        # return empty plot
        self.script.log (1, "StatReportingThread::parse_message [returning NO DATA YET]")
        self.script.results["lock"].release()
        return False, self.no_data

        #else:
        #  self.script.results["lock"].release()
        #  # still return if the timestamp is recent
        #  return False, self.no_data

      xml += "<stat_state>"
      xml += "<error>Invalid request</error>"
      xml += "</stat_state>\r\n"

      return True, xml

#################################################################
# thread for executing processing commands
class dbstatsThread (threading.Thread):

  def __init__ (self, cmd, dir, pipe, dl):
    threading.Thread.__init__(self)
    self.cmd = cmd
    self.pipe = pipe
    self.dir = dir
    self.dl = dl

  def run (self):
    cmd = "cd " + self.dir + "; " + self.cmd
    rval = system_piped (cmd, self.pipe, self.dl <= DL)
    return rval

class StatDaemon(Daemon,StreamBased):

  def __init__ (self, name, id):
    Daemon.__init__(self, name, str(id))
    StreamBased.__init__(self, id, self.cfg)

    self.processing_dir = self.cfg["CLIENT_STATS_DIR"] + "/processing"
    self.valid_plots = ["histogram", "freq_vs_time"]
    self.results = {}

    self.results["lock"] = threading.Lock()
    self.results["valid"] = False;

    (host, beam_id, subband_id) = self.cfg["STREAM_" + id].split(":")
    self.beam = self.cfg["BEAM_" + beam_id] 

    self.hg_plot = HistogramPlot()
    self.ft_plot = FreqTimePlot()

    self.hg_valid = False
    self.ft_valid = False
    self.ms_valid = False

  #################################################################
  # main
  #       id >= 0   process folded archives from a stream
  #       id == -1  process folded archives from all streams
  def main (self):

    # stats files are stored in flat directory structure
    #  beam / utc_start / stats

    if not os.path.exists(self.processing_dir):
      os.makedirs(self.processing_dir, 0755) 
    self.log (2, "StatDaemon::main stream_id=" + str(self.id))

    # get the data block keys
    db_prefix  = self.cfg["DATA_BLOCK_PREFIX"]
    db_id      = self.cfg["PROCESSING_DATA_BLOCK"]
    num_stream = self.cfg["NUM_STREAM"]
    db_key     = SMRBDaemon.getDBKey (db_prefix, stream_id, num_stream, db_id)
    self.log (2, "StatDaemon::main db_key=" + db_key)

    # start dbstats in a separate thread
    stat_dir   = self.cfg["CLIENT_STATS_DIR"]   + "/processing/" + str(self.id)

    if not os.path.exists(stat_dir):
      os.makedirs(stat_dir, 0755)

    # configure the histogram plot with all channels included
    self.hg_plot.configure (-1)

    # configure the freq v time plot
    log = False
    zap = False
    transpose = False
    self.ft_plot.configure (log, zap, transpose)

    log_host = self.cfg["SERVER_HOST"]
    log_port = int(self.cfg["SERVER_LOG_PORT"])

    # stat will use the stream config file created for the recv command
    stream_config_file = "/tmp/spip_stream_" + str(self.id) + ".cfg"
    while (not os.path.exists(stream_config_file)):
      self.log (2, "StatDaemon::main waiting for stream_config file [" + stream_config_file +"] to be created by recv")
      time.sleep(1)    

    # this stat command will not change from observation to observation
    stat_cmd = self.cfg["STREAM_STATS_BINARY"] + " -k " + db_key + " " + stream_config_file \
               + " -D  " + stat_dir

    while (not self.quit_event.isSet()):

      # create a log pipe for the stats command
      stat_log_pipe   = LogSocket ("stat_src", "stat_src", str(self.id), "stream",
                                   log_host, log_port, int(DL))

      # connect up the log file output
      stat_log_pipe.connect()

      # add this binary to the list of active commands
      self.binary_list.append (self.cfg["STREAM_STATS_BINARY"] + " -k " + db_key)

       # initialize the threads
      stat_thread = dbstatsThread (stat_cmd, stat_dir, stat_log_pipe.sock, 2)

      self.log (1, stat_cmd)

      self.log (2, "StatDaemon::main starting stat thread")
      stat_thread.start()
      self.log (2, "StatDaemon::main stat thread started")
     
      while stat_thread.is_alive():

        # get a list of all the recent observations
        observations = os.listdir (stat_dir)

        self.log (2, "StatDaemon::main observations=" + str(observations))

        # for each observation      
        for utc in observations:
   
          self.log (2, "StatDaemon::main utc=" + utc)

          utc_dir = stat_dir + "/" + utc

          # find the most recent HG stats file
          #hg_files = [file for file in os.listdir(utc_dir) if file.lower().endswith(".hg.stats")]

          #self.log (3, "StatDaemon::main hg_files=" + str(hg_files))
          #hg_file = hg_files[-1]
          #self.log (3, "main: hg_file=" + str(hg_file))

          # only 1 channel in the histogram
          #hg_fptr = open (utc_dir + "/" + str(hg_file), "rb")
         #npol = np.fromfile(hg_fptr, dtype=np.uint32, count=1)
          #ndim = np.fromfile(hg_fptr, dtype=np.uint32, count=1)
          #nbin = np.fromfile(hg_fptr, dtype=np.uint32, count=1)

          #self.log (3, "StatDaemon::main npol=" + str(npol) +" ndim=" + str(ndim) + " nbin=" + str(nbin))
          #hg_data = {}
          #for ipol in range(npol):
          #  hg_data[ipol] = {}
          #  for idim in range(ndim):
          #    hg_data[ipol][idim] = np.fromfile (hg_fptr, dtype=np.uint32, count=nbin)
          #hg_fptr.close()
            
          # read the most recent freq_vs_time stats file
          #ft_files = [file for file in os.listdir(utc_dir) if file.lower().endswith(".ft.stats")]
          #self.log (3, "StatDaemon::main ft_files=" + str(ft_files))
          #ft_file = ft_files[-1]
          #self.log (3, "StatDaemon::main ft_file=" + str(ft_file))

          #ft_fptr = open (utc_dir + "/" + str(ft_file), "rb")
          #npol = np.fromfile(ft_fptr, dtype=np.uint32, count=1)
          #nfreq = np.fromfile(ft_fptr, dtype=np.uint32, count=1)
          #ntime = np.fromfile(ft_fptr, dtype=np.uint32, count=1)
          #self.log (3, "StatDaemon::main npol=" + str(npol) + " nfreq=" + str(nfreq) + " ntime=" + str(ntime))
          
          #ft_data = {}
          #for ipol in range(npol):
          #  ft_data[ipol] = np.fromfile (ft_fptr, dtype=np.uint32, count=nfreq*ntime)
          #  ft_data[ipol].shape = (nfreq, ntime)
          #ft_fptr.close()

          # find the most recent Mean/Stddev stats files
          #ms_files = [file for file in os.listdir(utc_dir) if file.lower().endswith(".ms.stats")]
          
          #self.log (3, "StatDaemon::main ms_files=" + str(ms_files))
          #ms_file = ms_files[-1]
          #self.log (2, "StatDaemon::main ms_file=" + str(ms_file))
          #ms_fptr = open (utc_dir + "/" + str(ms_file), "rb")
          #npol = np.fromfile(ms_fptr, dtype=np.uint32, count=1)
          #ndim = np.fromfile(ms_fptr, dtype=np.uint32, count=1)

          #means = np.fromfile (ms_fptr, dtype=np.float32, count=npol*ndim)
          #stddevs = np.fromfile (ms_fptr, dtype=np.float32, count=npol*ndim)
          #ms_fptr.close()

          self.process_hg (utc_dir)
          self.process_ft (utc_dir)
          self.process_ms (utc_dir)

          self.results["lock"].acquire()
          self.results["timestamp"] = times.getCurrentTime()
          self.results["valid"] = self.hg_valid and self.ft_valid and self.ms_valid

          #self.hg_plot.plot_binned (240, 180, True, hg_data[0][0], hg_data[0][1], nbin)
          #self.results["histogram_0"] = self.hg_plot.getRawImage()

          #self.hg_plot.plot_binned (240, 180, True, hg_data[1][0], hg_data[1][1], nbin)
          #self.results["histogram_1"] = self.hg_plot.getRawImage()

          #self.ft_plot.plot (240, 180, True, ft_data[0], nfreq, ntime)
          #self.results["freq_vs_time_0"] = self.ft_plot.getRawImage()

          #self.ft_plot.plot (240, 180, True, ft_data[1], nfreq, ntime)
          #self.results["freq_vs_time_1"] = self.ft_plot.getRawImage()

          #self.results["hg_mean_0_re"] = means[0]
          #self.results["hg_mean_0_im"] = means[1]
          #self.results["hg_stddev_0_re"] = stddevs[0]
          #self.results["hg_stddev_0_im"] = stddevs[1]
          #self.results["hg_mean_1_re"] = means[2]
          #self.results["hg_mean_1_im"] = means[3]
          #self.results["hg_stddev_1_re"] = stddevs[2]
          #self.results["hg_stddev_1_im"] = stddevs[3]
          self.results["lock"].release()


        time.sleep(5)

      self.log (2, "StatDaemon::main joining stat thread")
      rval = stat_thread.join()
      self.log (2, "StatDaemon::main stat thread joined")
      if rval:
        self.log (-2, "stat thread failed")
        self.quit_event.set()

  
  def process_hg (self, utc_dir):

    # find the most recent HG stats file
    files = [file for file in os.listdir(utc_dir) if file.lower().endswith(".hg.stats")]

    if len(files) > 0:

      self.log (3, "StatDaemon::process_hg files=" + str(files))
      hg_file = files[-1]
      self.log (2, "StatDaemon::process_hg hg_file=" + str(hg_file))

      # only 1 channel in the histogram
      hg_fptr = open (utc_dir + "/" + str(hg_file), "rb")
      npol = np.fromfile(hg_fptr, dtype=np.uint32, count=1)
      ndim = np.fromfile(hg_fptr, dtype=np.uint32, count=1)
      nbin = np.fromfile(hg_fptr, dtype=np.uint32, count=1)

      self.log (3, "StatDaemon::process_hg npol=" + str(npol) +" ndim=" + str(ndim) + " nbin=" + str(nbin))
      hg_data = {}
      for ipol in range(npol):
        hg_data[ipol] = {}
        for idim in range(ndim):
          hg_data[ipol][idim] = np.fromfile (hg_fptr, dtype=np.uint32, count=nbin)
      hg_fptr.close()

      self.results["lock"].acquire()
      self.hg_plot.plot_binned (240, 180, True, hg_data[0][0], hg_data[0][1], nbin)
      self.results["histogram_0"] = self.hg_plot.getRawImage()
      self.hg_plot.plot_binned (240, 180, True, hg_data[1][0], hg_data[1][1], nbin)
      self.results["histogram_1"] = self.hg_plot.getRawImage()
      self.hg_plot.plot_binned4 (240, 180, True, hg_data[0][0], hg_data[0][1], hg_data[1][0], hg_data[1][1], nbin)
      self.results["histogram_s"] = self.hg_plot.getRawImage()

      self.hg_valid = True
      self.results["lock"].release()

      for file in files:
        os.remove (utc_dir + "/" + file)


  def process_ft (self, utc_dir):
    
    self.log (2, "StatDaemon::process_ft("+utc_dir+")")

    # read the most recent freq_vs_time stats file
    files = [file for file in os.listdir(utc_dir) if file.lower().endswith(".ft.stats")]
    if len(files) > 0:

      self.log (3, "StatDaemon::process_ft files=" + str(files))
      ft_file = files[-1]
      self.log (2, "StatDaemon::process_ft ft_file=" + str(ft_file))

      ft_fptr = open (utc_dir + "/" + str(ft_file), "rb")
      npol = np.fromfile(ft_fptr, dtype=np.uint32, count=1)
      nfreq = np.fromfile(ft_fptr, dtype=np.uint32, count=1)
      ntime = np.fromfile(ft_fptr, dtype=np.uint32, count=1)
      self.log (3, "StatDaemon::process_ft npol=" + str(npol) + " nfreq=" + str(nfreq) + " ntime=" + str(ntime))

      ft_data = {}
      for ipol in range(npol):
        ft_data[ipol] = np.fromfile (ft_fptr, dtype=np.uint32, count=nfreq*ntime)
        ft_data[ipol].shape = (nfreq, ntime)
      ft_fptr.close()

      ft_summed = np.add(ft_data[0], ft_data[1])

      self.log (3, "StatDaemon::process_ft plotting")
      self.results["lock"].acquire()

      self.ft_plot.plot (240, 180, True, ft_data[0], nfreq, ntime)
      self.results["freq_vs_time_0"] = self.ft_plot.getRawImage()
      self.ft_plot.plot (240, 180, True, ft_data[1], nfreq, ntime)
      self.results["freq_vs_time_1"] = self.ft_plot.getRawImage()
      self.ft_plot.plot (240, 180, True, ft_data[0], nfreq, ntime)
      self.results["freq_vs_time_s"] = self.ft_plot.getRawImage()
      
      self.ft_valid = True
      self.results["lock"].release()

      for file in files:
        os.remove (utc_dir + "/" + file)

  def process_ms (self, utc_dir):

    self.log (2, "StatDaemon::process_ms("+utc_dir+")")

    # find the most recent Mean/Stddev stats files
    files = [file for file in os.listdir(utc_dir) if file.lower().endswith(".ms.stats")]
    if len(files) > 0:

      self.log (3, "StatDaemon::process_ms files=" + str(files))
      ms_file = files[-1]
      self.log (2, "StatDaemon::process_ms ms_file=" + str(ms_file))
      ms_fptr = open (utc_dir + "/" + str(ms_file), "rb")
      npol = np.fromfile(ms_fptr, dtype=np.uint32, count=1)
      ndim = np.fromfile(ms_fptr, dtype=np.uint32, count=1)

      means = np.fromfile (ms_fptr, dtype=np.float32, count=npol*ndim)
      stddevs = np.fromfile (ms_fptr, dtype=np.float32, count=npol*ndim)
      ms_fptr.close()

      self.results["lock"].acquire()
      self.results["hg_mean_0_re"] = means[0]
      self.results["hg_mean_0_im"] = means[1]
      self.results["hg_stddev_0_re"] = stddevs[0]
      self.results["hg_stddev_0_im"] = stddevs[1]
      self.results["hg_mean_1_re"] = means[2]
      self.results["hg_mean_1_im"] = means[3]
      self.results["hg_stddev_1_re"] = stddevs[2]
      self.results["hg_stddev_1_im"] = stddevs[3]
      self.ms_valid = True
      self.results["lock"].release()

      for file in files:
        os.remove (utc_dir + "/" + file)


###############################################################################

if __name__ == "__main__":

  if len(sys.argv) != 2:
    print "ERROR: 1 command line argument expected"
    sys.exit(1)

  # this should come from command line argument
  stream_id = sys.argv[1]

  script = []
  script = StatDaemon ("spip_stat", stream_id)

  state = script.configure (DAEMONIZE, DL, "stat", "stat") 

  if state != 0:
    sys.exit(state)

  script.log(1, "STARTING SCRIPT")

  try:

    reporting_thread = StatReportingThread(script, stream_id)
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

