
noinst_LTLIBRARIES = libmeerkat.la

libmeerkat_headers = spip/UDPFormatMeerKATSimple.h spip/BlockFormatMeerKAT.h

libmeerkat_la_SOURCES = UDPFormatMeerKATSimple.C BlockFormatMeerKAT.C

bin_PROGRAMS = meerkat_udpgen meerkat_udprecv

meerkat_udpgen_SOURCES = meerkat_udpgen.C
meerkat_udprecv_SOURCES = meerkat_udprecv.C

AM_CXXFLAGS = -I. \
	-I$(top_builddir)/src/Affinity \
	-I$(top_builddir)/src/Network \
	-I$(top_builddir)/src/Util

LDADD = libmeerkat.la \
	$(top_builddir)/src/libspip.la -lpthread 

if HAVE_PSRDADA

bin_PROGRAMS += meerkat_udpdb meerkat_dbstats meerkat_simdb meerkat_udpmergedb
AM_CXXFLAGS += -I$(top_builddir)/src/Dada

meerkat_udpdb_SOURCES = meerkat_udpdb.C
meerkat_dbstats_SOURCES = meerkat_dbstats.C
meerkat_simdb_SOURCES = meerkat_simdb.C
meerkat_udpmergedb_SOURCES = meerkat_udpmergedb.C

endif

if HAVE_SPEAD2

libmeerkat_headers += spip/UDPFormatMeerKATSPEAD.h

libmeerkat_la_SOURCES += UDPFormatMeerKATSPEAD.C

bin_PROGRAMS += meerkat_speadrecv meerkat_speadrecvmerge meerkat_speadmeta meerkat_speadtimestamp

meerkat_speadrecv_SOURCES = meerkat_speadrecv.C
meerkat_speadrecvmerge_SOURCES = meerkat_speadrecvmerge.C
meerkat_speadmeta_SOURCES = meerkat_speadmeta.C
meerkat_speadtimestamp_SOURCES = meerkat_speadtimestamp.C

if HAVE_PSRDADA 

bin_PROGRAMS += meerkat_speaddb
meerkat_speaddb_SOURCES = meerkat_speaddb.C

endif

endif

AM_CXXFLAGS += @PSRDADA_CFLAGS@ @HWLOC_CFLAGS@ @BOOST_CPPFLAGS@ @SPEAD2_CFLAGS@ @CUDA_CFLAGS@
LDADD += @PSRDADA_LIBS@ @HWLOC_LIBS@ @BOOST_SYSTEM_LDFLAGS@ @SPEAD2_LIBS@
