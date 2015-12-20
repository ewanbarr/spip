
lib_LTLIBRARIES = libmeerkat.la

libmeerkat_headers = spip/UDPFormatMeerKATSimple.h spip/BlockFormatMeerKAT.h

libmeerkat_la_SOURCES = UDPFormatMeerKATSimple.C BlockFormatMeerKAT.C

bin_PROGRAMS = meerkat_udpdb meerkat_udpgen meerkat_udprecv meerkat_dbstats

AM_CXXFLAGS = -I. -I$(top_builddir)/src/Network -I$(top_builddir)/src/Dada 

LDADD = libmeerkat.la $(top_builddir)/src/Dada/libspipdada.la $(top_builddir)/src/Network/libspipnet.la 

if HAVE_PSRDADA

AM_CXXFLAGS += @PSRDADA_CFLAGS@

LDADD += @PSRDADA_LIBS@

endif

if HAVE_SPEAD2

AM_CXXFLAGS += @SPEAD2_CFLAGS@

LDADD += @SPEAD2_LIBS@

libmeerkat_headers += spip/UDPFormatMeerKATSPEAD.h

libmeerkat_la_SOURCES += UDPFormatMeerKATSPEAD.C

bin_PROGRAMS += meerkat_speadrecv

endif
