
noinst_LTLIBRARIES = libcaspsr.la

libcaspsr_headers = spip/UDPFormatCASPSR.h spip/BlockFormatCASPSR.h

libcaspsr_la_SOURCES = UDPFormatCASPSR.C BlockFormatCASPSR.C

bin_PROGRAMS = caspsr_dbstats caspsr_udpmergedb caspsr_udprecv caspsr_utctimestamp

caspsr_dbstats_SOURCES = caspsr_dbstats.C
caspsr_udpmergedb_SOURCES = caspsr_udpmergedb.C
caspsr_udprecv_SOURCES = caspsr_udprecv.C
caspsr_utctimestamp_SOURCES = caspsr_utctimestamp.C

AM_CXXFLAGS = -I. \
	-I$(top_builddir)/src/Affinity \
	-I$(top_builddir)/src/Network \
	-I$(top_builddir)/src/Util \
	-I$(top_builddir)/src/Dada

LDADD = libcaspsr.la \
	$(top_builddir)/src/Network/libspipnet.la \
	$(top_builddir)/src/Dada/libspipdada.la \
	$(top_builddir)/src/Util/libspiputil.la \
	$(top_builddir)/src/libspip.la -lpthread 

nodist_EXTRA_libcaspsr_la_SOURCES = dummy.cxx

AM_CXXFLAGS += @PSRDADA_CFLAGS@ @HWLOC_CFLAGS@ @CUDA_CFLAGS@
LDADD += @PSRDADA_LIBS@ @HWLOC_LIBS@ @BOOST_SYSTEM_LDFLAGS@ @SPEAD2_LIBS@ 
