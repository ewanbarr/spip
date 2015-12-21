
lib_LTLIBRARIES = libska1.la

libska1_headers = spip/UDPFormatCustom.h

libska1_la_SOURCES = UDPFormatCustom.C

bin_PROGRAMS = ska1_udpgen ska1_udprecv ska1_udpdb

AM_CXXFLAGS = -I. \
	-I$(top_builddir)/src/Network \
	-I$(top_builddir)/src/Dada \
  -I$(top_builddir)/src/Affinity

LDADD = libska1.la \
	$(top_builddir)/src/Network/libspipnet.la \
	$(top_builddir)/src/Dada/libspipdada.la \
  $(top_builddir)/src/Affinity/libspipaffinity.la \
	-L/opt/cross/lib 

AM_CXXFLAGS += @PSRDADA_CFLAGS@ @HWLOC_CFLAGS@ @SPEAD2_CFLAGS@
LDADD += @PSRDADA_LIBS@ @HWLOC_LIBS@ @SPEAD2_LIBS@
