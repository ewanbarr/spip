
lib_LTLIBRARIES = libmeerkat.la

libmeerkat_headers = spip/UDPFormatMeerKATSimple.h spip/BlockFormatMeerKAT.h

libmeerkat_la_SOURCES = UDPFormatMeerKATSimple.C BlockFormatMeerKAT.C

bin_PROGRAMS = meerkat_udpgen meerkat_udprecv

AM_CXXFLAGS = -I. \
	-I$(top_builddir)/src/Affinity \
	-I$(top_builddir)/src/Network \
	-I$(top_builddir)/src/Util

LDADD = libmeerkat.la \
	$(top_builddir)/src/Affinity/libspipaffinity.la

if HAVE_PSRDADA

bin_PROGRAMS += meerkat_udpdb meerkat_dbstats
AM_CXXFLAGS += -I$(top_builddir)/src/Dada
LDADD += $(top_builddir)/src/Dada/libspipdada.la
  
endif

if HAVE_SPEAD2

libmeerkat_headers += spip/UDPFormatMeerKATSPEAD.h

libmeerkat_la_SOURCES += UDPFormatMeerKATSPEAD.C

bin_PROGRAMS += meerkat_speadrecv meerkat_speadrecvmerge meerkat_speaddb

if HAVE_PSRDADA 

bin_PROGRAMS += meerkat_speaddb

endif

endif

LDADD += $(top_builddir)/src/Network/libspipnet.la \
				$(top_builddir)/src/Util/libspiputil.la \
				-L/opt/cross/lib -lpthread

AM_CXXFLAGS += @PSRDADA_CFLAGS@ @HWLOC_CFLAGS@ @SPEAD2_CFLAGS@
LDADD += @PSRDADA_LIBS@ @HWLOC_LIBS@ @SPEAD2_LIBS@
