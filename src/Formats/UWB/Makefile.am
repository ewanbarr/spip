
noinst_LTLIBRARIES = libuwb.la

libuwb_headers = spip/BlockFormatUWB.h

libuwb_la_SOURCES = BlockFormatUWB.C

bin_PROGRAMS = uwb_dbstats uwb_udprecv uwb_udpdb uwb_simdb

uwb_dbstats_SOURCES = uwb_dbstats.C
uwb_udprecv_SOURCES = uwb_udprecv.C
uwb_udpdb_SOURCES = uwb_udpdb.C
uwb_simdb_SOURCES = uwb_simdb.C

AM_CXXFLAGS = -I. \
	-I$(top_builddir)/src/Affinity \
	-I$(top_builddir)/src/Network \
	-I$(top_builddir)/src/Util \
	-I$(top_builddir)/src/Dada \
	-I$(top_builddir)/src/Formats/VDIF

LDADD = libuwb.la \
	$(top_builddir)/src/Network/libspipnet.la \
	$(top_builddir)/src/Dada/libspipdada.la \
	$(top_builddir)/src/Util/libspiputil.la \
	$(top_builddir)/src/Formats/VDIF/libvdif.la \
	$(top_builddir)/src/libspip.la -lpthread 

AM_CXXFLAGS += @PSRDADA_CFLAGS@ @HWLOC_CFLAGS@ @CUDA_CFLAGS@
LDADD += @PSRDADA_LIBS@ @HWLOC_LIBS@ @BOOST_SYSTEM_LDFLAGS@ @SPEAD2_LIBS@
