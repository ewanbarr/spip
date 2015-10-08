
lib_LTLIBRARIES = libmeerkat.la

libmeerkat_headers = spip/UDPFormatMeerKATSimple.h

libmeerkat_la_SOURCES = UDPFormatMeerKATSimple.C

#bin_PROGRAMS = meerkat_udpgen meerkat_udprecv meerkat_udpdb
bin_PROGRAMS = meerkat_udpdb meerkat_udpgen

AM_CXXFLAGS = -I. -I$(top_builddir)/src/Network -I$(top_builddir)/src/Dada -I/home/spip/linux_64/include

LDADD = libmeerkat.la -L/home/dada/linux_64/lib $(top_builddir)/src/Network/libspipnet.la $(top_builddir)/src/Dada/libspipdada.la -lpsrdada

