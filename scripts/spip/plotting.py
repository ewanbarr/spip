##############################################################################
#  
#     Copyright (C) 2015 by Andrew Jameson
#     Licensed under the Academic Free License version 2.1
# 
###############################################################################

import StringIO, numpy, fnmatch

import matplotlib
matplotlib.use('agg')
import pylab 
from matplotlib.backends.backend_agg import FigureCanvasAgg as FigureCanvas
import matplotlib.cm as cm
from matplotlib.colors import LogNorm

class InlinePlot (object):

  def __init__(self):
    self.xres = 800
    self.yres = 600
    self.res_set = False
    self.log = False
    self.plain_plot = False
    self.title = ''
    self.xlabel = ''
    self.ylabel = ''
    self.imgdata = StringIO.StringIO()
    self.fig = matplotlib.figure.Figure(facecolor='black')
    self.dpi = self.fig.get_dpi()
    self.ax = []

  def setTranspose (self, val):
    self.log = val;

  def setLog (self, val):
    self.log = val;

  def setPlain (self, val):
    self.plain_plot = val;

  def setZap(self, val):
    self.zap = val;

  # change the resolution for this plot
  def setResolution (self, xres, yres):

    # if the resolution is different
    if ((xres != self.xres) and (yres != self.yres) and not self.res_set):
      xinches = float(xres) / float(self.dpi)
      yinches = float(yres) / float(self.dpi)
      self.fig.set_size_inches((xinches, yinches))

      # save the new/current resolution
      self.xres = xres
      self.yres = yres
      self.res_set = True

  def setLabels (self, title='', xlabel='', ylabel=''):
    self.title = title
    self.xlabel = xlabel
    self.ylabel = ylabel

  # start plotting a new imagea
  def openPlot (self, xres, yres, plain):

    # ensure plot is of the right size
    #print "InlinePlot::openPlot setResolution ("+str(xres)+", "+str(yres)+")"
    self.setResolution (xres, yres)

    # always add axes on a new plot [TODO check!]
    if plain:
      self.ax = self.fig.add_axes((0,0,1,1))
    else:
      self.ax = self.fig.add_subplot(1,1,1)

    set_foregroundcolor(self.ax, 'black')
    set_backgroundcolor(self.ax, 'white')

    if not plain:
      self.ax.set_title(self.title)
      self.ax.set_xlabel(self.xlabel)
      self.ax.set_ylabel(self.ylabel)

    self.ax.grid(False)

  def showPlot (self):
    FigureCanvas(self.fig).savefig('test.png')

  def closePlot (self):
    FigureCanvas(self.fig).print_png(self.imgdata)
    self.fig.delaxes(self.ax)
    self.imgdata.seek(0)
    self.fig.clf()

  def getRawImage (self):
    return self.imgdata.buf

# plot a histogram:
class HistogramPlot (InlinePlot):

  def __init__(self):
    super(HistogramPlot, self).__init__()
    self.configure (-1)
    self.setLabels ('Histogram', '', '')

  def configure (self, channel):
    if channel == -1:
      self.setLabels ('Histogram all channels', '', '') 
    else:
      self.setLabels ('Histogram channel '+str(channel), '', '') 

  def plot_binned (self, xres, yres, plain, real, imag, nbins):
    self.openPlot(xres, yres, plain)
    self.bins = numpy.arange(-128,128,1)
    self.ax.plot(self.bins, real, color='red', label='real')
    self.ax.plot(self.bins, imag, color='green', label='imag')
    self.closePlot()

  def plot_binned_image (self, xres, yres, plain, data, nfreq, nbins):
    self.openPlot(xres, yres, plain)
    self.ax.imshow(data, extent=(0, nbins, 0, nfreq), aspect='auto',
                   origin='lower', interpolation='nearest',
                   cmap=cm.get_cmap('summer'))
    self.closePlot()

  def plot_binned4 (self, xres, yres, plain, real0, imag0, real1, imag1, nbins):
    self.openPlot(xres, yres, plain)
    self.bins = numpy.arange(-128,128,1)
    self.ax.plot(self.bins, real0, color='red', label='real0')
    self.ax.plot(self.bins, imag0, color='green', label='imag0')
    self.ax.plot(self.bins, real1, color='yellow', label='real1')
    self.ax.plot(self.bins, imag1, color='blue', label='imag1')
    self.closePlot()

  def plot (self, xres, yres, plain, real, imag, nbins):
    self.openPlot(xres, yres, plain)
    if len(real) > 0:
      self.ax.hist(real, nbins, range=[-128, 127], color='red', label='real', histtype='step')
    if len(imag) > 0:
      self.ax.hist(imag, nbins, range=[-128, 127], color='green', label='imag', histtype='step' )
    self.closePlot()

# plot a complex timeseries
class TimeseriesPlot (InlinePlot):

  def __init__(self):
    super(TimeseriesPlot, self).__init__()
    self.nsamps = 0
    self.configure (64, self.nsamps)
    self.samps = numpy.arange (self.nsamps)

  def configure (self, channel, nsamps):
    self.setLabels ('Timeseries Channel ' + str(channel), 'Time (samples)', 'States (Voltages)')
    if self.nsamps != nsamps:
      self.nsamps = nsamps
      self.samps = numpy.arange (self.nsamps)
 
  def plot (self, xres, yres, plain, real, imag):
    self.openPlot(xres, yres, plain)

    if len(real) > 0:
      self.ax.plot(self.samps, real, c='r', marker=',', linestyle='None', label='real')
    if len(imag) > 0:
      self.ax.plot(self.samps, imag, c='g', marker=',', linestyle='None', label='imag')

    self.ax.set_xlim((0, self.nsamps))
    self.ax.set_ylim((-128.0, 127.0))

    self.closePlot()

class SNRPlot (InlinePlot):

  def __init__(self):
    super(SNRPlot, self).__init__()
    self.configure ()

  def configure (self):
    self.setLabels ('SNR Ratio for Observation', 'Time (seconds)', 'SNR')

  def plot (self, xres, yres, plain, x, y):
    self.openPlot(xres, yres, plain)

    self.ax.plot(x, y)

    self.closePlot()


class BandpassPlot (InlinePlot):

  def __init__(self):
    super(BandpassPlot, self).__init__()
    self.setLabels ('Bandpass', 'Channel', 'Power')
    self.nchan = 0
    self.xvals = numpy.arange (self.nchan)
    self.configure (False, False, False, 0)

  def configure (self, log, zap, transpose, nchan):
    self.log = log
    self.zap = zap
    self.transpose = transpose
    if (self.nchan != nchan):
      self.nchan = nchan
      self.xvals = numpy.arange(self.nchan)

  def plot (self, xres, yres, plain, nchan, spectrum):
    self.openPlot (xres, yres, plain)
    if self.log: 
      self.ax.set_yscale ('log', nonposy='clip')
    else:
      self.ax.set_yscale ('linear')
    if self.zap:
      spectrum[0] = 0
    if self.transpose:
      self.ax.plot(spectrum, self.xvals, c='w')
      self.ax.set_ylim((0, nchan))
    else:
      ymin = numpy.amin(spectrum)
      ymax = numpy.amax(spectrum)
      if ymax == ymin:
        ymax = ymin + 1
        spectrum[0] = 1
      self.ax.plot(self.xvals, spectrum, c='w')
      self.ax.set_xlim((0, nchan))
      self.ax.set_ylim((ymin, ymax))

    self.closePlot()

class FreqTimePlot (InlinePlot):

  def __init__(self):
    super(FreqTimePlot, self).__init__()
    self.setLabels ('Waterfall', 'Time (sample)', 'Channel')
    self.configure (False, False, False)

  def configure (self,log, zap, transpose):
    self.log = log
    self.zap = zap
    self.transpose = transpose

  def plot (self, xres, yres, plain, spectra, nchan, nsamps):
    self.openPlot(xres, yres, plain)

    if numpy.amax(spectra) == numpy.amin(spectra):
      spectra[0][0] = 1

    if self.zap:
      spectra[0,:] = 0

    if self.log:
      vmax = numpy.log(numpy.amax(spectra))
      self.ax.imshow(spectra, extent=(0, nsamps, 0, nchan), aspect='auto', 
                     origin='lower', interpolation='nearest', norm=LogNorm(vmin=0.0001,vmax=vmax), 
                     cmap=cm.get_cmap('gray'))
    else:
      self.ax.imshow(spectra, extent=(0, nsamps, 0, nchan), aspect='auto', 
                     origin='lower', interpolation='nearest', 
                     cmap=cm.get_cmap('gray'))

    self.closePlot()


def printBandpass (nchan, spectra):
  for ichan in range(nchan):
    Dada.logMsg(0, 2, '[' + str(ichan) + '] = ' + str(spectra[ichan]))


# creates a figure of the specified size
def createFigure(xdim, ydim):

  fig = matplotlib.figure.Figure(facecolor='black')
  dpi = fig.get_dpi()
  curr_size = fig.get_size_inches()
  xinches = float(xdim) / float(dpi)
  yinches = float(ydim) / float(dpi)
  fig.set_size_inches((xinches, yinches))
  return fig


def set_foregroundcolor(ax, color):
  for tl in ax.get_xticklines() + ax.get_yticklines():
    tl.set_color(color)
  for spine in ax.spines:
    ax.spines[spine].set_edgecolor(color)
  for tick in ax.xaxis.get_major_ticks():
    tick.label1.set_color(color)
  for tick in ax.yaxis.get_major_ticks():
    tick.label1.set_color(color)
  ax.axes.xaxis.label.set_color(color)
  ax.axes.yaxis.label.set_color(color)
  ax.axes.xaxis.get_offset_text().set_color(color)
  ax.axes.yaxis.get_offset_text().set_color(color)
  ax.axes.title.set_color(color)
  lh = ax.get_legend()
  if lh != None:
    lh.get_title().set_color(color)
    lh.legendPatch.set_edgecolor('none')
    labels = lh.get_texts()
    for lab in labels:
      lab.set_color(color)
  for tl in ax.get_xticklabels():
    tl.set_color(color)
  for tl in ax.get_yticklabels():
    tl.set_color(color)


def set_backgroundcolor(ax, color):
     ax.patch.set_facecolor(color)
     ax.set_axis_bgcolor(color)
     lh = ax.get_legend()
     if lh != None:
         lh.legendPatch.set_facecolor(color)

