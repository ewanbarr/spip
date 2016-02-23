
lib_LTLIBRARIES = libkat7.la

libkat7_headers = spip/BlockFormatKAT7.h

libkat7_la_SOURCES = BlockFormatKAT7.C


AM_CXXFLAGS = -I. \
	-I$(top_builddir)/src/Affinity \
	-I$(top_builddir)/src/Network \
	-I$(top_builddir)/src/Util

LDADD = libkat7.la \
	$(top_builddir)/src/Affinity/libspipaffinity.la \
	$(top_builddir)/src/Network/libspipnet.la \
	$(top_builddir)/src/Util/libspiputil.la \
	-L/opt/cross/lib

if HAVE_PSRDADA

AM_CXXFLAGS += -I$(top_builddir)/src/Dada
LDADD += $(top_builddir)/src/Dada/libspipdada.la

bin_PROGRAMS = kat7_dbstats
kat7_dbstats_SOURCES = kat7_dbstats.C

endif


AM_CXXFLAGS += @PSRDADA_CFLAGS@ @HWLOC_CFLAGS@ @SPEAD2_CFLAGS@
LDADD += @PSRDADA_LIBS@ @HWLOC_LIBS@ @SPEAD2_LIBS@

