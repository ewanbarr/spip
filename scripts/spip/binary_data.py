##############################################################################
#  
#     Copyright (C) 2015 by Andrew Jameson
#     Licensed under the Academic Free License version 2.1
# 
###############################################################################

import struct, math, time, os, sys, StringIO
import numpy, fnmatch

def unpack (nchan, nsamp, ndim, raw):
  nelement = nchan * nsamp * ndim
  format = str(nelement) + 'b'
  complex = numpy.array (struct.unpack_from(format, raw), dtype=numpy.float32).view(numpy.complex64)
  return complex.reshape((nchan, nsamp), order='F')

# extract data for histogramming
def extractChannel (channel, nchan, nsamp, dimtype, raw):

  real = []
  imag = []

  if dimtype == 'real' or dimtype == 'both':
    if channel == -1:
      real = raw.real.flatten()
    else:
      real = raw[channel,:].real

  if dimtype == 'imag' or dimtype == 'both':
    if channel == -1:
      imag = raw.imag.flatten()
    else:
      imag = raw[channel,:].imag
  
  return (real, imag)

def detectAndIntegrateInPlace (spectra, nchan, nsamp, ndim, raw):
  absolute = numpy.square(numpy.absolute(raw))
  spectra = numpy.sum(absolute, axis=0)

# square law detect, produce bandpass
def detectAndIntegrate (nchan, nsamp, ndim, raw):
  absolute = numpy.square(numpy.absolute(raw))
  spectrum = numpy.sum(absolute, axis=1)
  return spectrum

# square law detect, produce waterfall
def detect (nchan, nsamp, ndim, raw):
  absolute = numpy.square(numpy.absolute(raw))
  spectra = numpy.sum(absolute, axis=0)

# square law detect, produce waterfall
def detectTranspose (nchan, nsamp, ndim, raw):
  absolute = numpy.square(numpy.absolute(raw))
  return absolute
