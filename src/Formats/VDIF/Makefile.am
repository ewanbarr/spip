
noinst_LTLIBRARIES = libvdif.la

libvdif_headers = spip/UDPFormatVDIF.h spip/vdifio.h

libvdif_la_SOURCES = UDPFormatVDIF.C vdifio.c

bin_PROGRAMS = vdif_udprecv vdif_udpgen

vdif_udprecv_SOURCES = vdif_udprecv.C
vdif_udpgen_SOURCES = vdif_udpgen.C

AM_CXXFLAGS = -I. \
	-I$(top_builddir)/src/Affinity \
	-I$(top_builddir)/src/Network \
	-I$(top_builddir)/src/Util \
	-I$(top_builddir)/src/Dada

LDADD = libvdif.la \
	$(top_builddir)/src/Network/libspipnet.la \
	$(top_builddir)/src/Dada/libspipdada.la \
	$(top_builddir)/src/Util/libspiputil.la \
	$(top_builddir)/src/libspip.la -lpthread 

AM_CXXFLAGS += @PSRDADA_CFLAGS@ @HWLOC_CFLAGS@
LDADD += @PSRDADA_LIBS@ @HWLOC_LIBS@
