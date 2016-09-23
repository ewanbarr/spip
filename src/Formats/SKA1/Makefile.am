
noinst_LTLIBRARIES = libska1.la

libska1_headers = spip/UDPFormatCustom.h

libska1_la_SOURCES = UDPFormatCustom.C

bin_PROGRAMS = ska1_udpgen ska1_udprecv

ska1_udpgen_SOURCES = ska1_udpgen.C
ska1_udprecv_SOURCES = ska1_udprecv.C

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

ska1_udpdb_SOURCES = ska1_udpdb.C

endif

AM_CXXFLAGS += @HWLOC_CFLAGS@ @PSRDADA_CFLAGS@ @BOOST_CPPFLAGS@ @SPEAD2_CFLAGS@ @CUDA_CFLAGS@
LDADD += @HWLOC_LIBS@ @PSRDADA_LIBS@ @BOOST_SYSTEM_LDFLAGS@ @SPEAD2_LIBS@
