
noinst_LTLIBRARIES = libmeerkat.la

libmeerkat_headers = spip/UDPFormatMeerKATSimple.h spip/BlockFormatMeerKAT.h

libmeerkat_la_SOURCES = UDPFormatMeerKATSimple.C BlockFormatMeerKAT.C

bin_PROGRAMS = meerkat_udpgen meerkat_udprecv

AM_CXXFLAGS = -I. \
	-I$(top_builddir)/src/Affinity \
	-I$(top_builddir)/src/Network \
	-I$(top_builddir)/src/Util

LDADD = libmeerkat.la \
	$(top_builddir)/src/libspip.la -lpthread 

if HAVE_PSRDADA

bin_PROGRAMS += meerkat_udpdb meerkat_dbstats meerkat_simdb meerkat_udpmergedb
AM_CXXFLAGS += -I$(top_builddir)/src/Dada

endif

if HAVE_SPEAD2

libmeerkat_headers += spip/UDPFormatMeerKATSPEAD.h

libmeerkat_la_SOURCES += UDPFormatMeerKATSPEAD.C

bin_PROGRAMS += meerkat_speadrecv meerkat_speadrecvmerge meerkat_speaddb meerkat_speadmeta meerkat_speadtimestamp

if HAVE_PSRDADA 

bin_PROGRAMS += meerkat_speaddb

endif

endif

AM_CXXFLAGS += @PSRDADA_CFLAGS@ @HWLOC_CFLAGS@ @BOOST_CPPFLAGS@ @SPEAD2_CFLAGS@
LDADD += @PSRDADA_LIBS@ @HWLOC_LIBS@ @BOOST_SYSTEM_LDFLAGS@ @SPEAD2_LIBS@
