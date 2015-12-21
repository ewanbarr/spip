
lib_LTLIBRARIES = libmeerkat.la

libmeerkat_headers = spip/UDPFormatMeerKATSimple.h spip/UDPFormatMeerKATSPEAD.h

libmeerkat_la_SOURCES = UDPFormatMeerKATSimple.C UDPFormatMeerKATSPEAD.C

bin_PROGRAMS = meerkat_udpdb meerkat_udpgen meerkat_udprecv meerkat_dbstats meerkat_speadrecv meerkat_speaddb

AM_CXXFLAGS = -I. -I$(top_builddir)/src/Network -I$(top_builddir)/src/Dada -I/home/spip/linux_64/include -std=c++11

LDADD = libmeerkat.la -L/home/spip/linux_64/lib $(top_builddir)/src/Network/libspipnet.la $(top_builddir)/src/Dada/libspipdada.la -lpsrdada -L/home/dada/linux_64/lib -lspead2 -L/opt/cross/lib -lboost_system

