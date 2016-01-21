
lib_LTLIBRARIES = libkat7.la

libkat7_headers = spip/BlockFormatKAT7.h

libkat7_la_SOURCES = BlockFormatKAT7.C

bin_PROGRAMS = kat7_dbstats

kat7_dbstats_SOURCES = kat7_dbstats.C

AM_CXXFLAGS = -I. \
	-I$(top_builddir)/src/Network \
	-I$(top_builddir)/src/Dada \
	-I$(top_builddir)/src/Affinity

LDADD = libkat7.la \
	$(top_builddir)/src/Dada/libspipdada.la \
	$(top_builddir)/src/Network/libspipnet.la \
	$(top_builddir)/src/Affinity/libspipaffinity.la \
	-L/opt/cross/lib

AM_CXXFLAGS += @PSRDADA_CFLAGS@ @HWLOC_CFLAGS@ @SPEAD2_CFLAGS@
LDADD += @PSRDADA_LIBS@ @HWLOC_LIBS@ @SPEAD2_LIBS@

