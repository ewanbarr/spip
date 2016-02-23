
noinst_LTLIBRARIES = libska1.la

libska1_headers = spip/UDPFormatCustom.h

libska1_la_SOURCES = UDPFormatCustom.C

bin_PROGRAMS = ska1_udpgen ska1_udprecv

AM_CXXFLAGS = -I. \
  -I$(top_builddir)/src/Affinity \
	-I$(top_builddir)/src/Network \
	-I$(top_builddir)/src/Util

LDADD = libska1.la \
  $(top_builddir)/src/libspip.la \
	-lpthread

if HAVE_PSRDADA

bin_PROGRAMS += ska1_udpdb
AM_CXXFLAGS += -I$(top_builddir)/src/Dada

endif

AM_CXXFLAGS += @HWLOC_CFLAGS@ @SPEAD2_CFLAGS@ @PSRDADA_CFLAGS@
LDADD += @HWLOC_LIBS@ @SPEAD2_LIBS@ @PSRDADA_LIBS@
