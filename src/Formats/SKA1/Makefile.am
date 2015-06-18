
lib_LTLIBRARIES = libska1.la

libska1_headers = spip/CustomUDPGenerator.h spip/CustomUDPReceiver.h

libska1_la_SOURCES = CustomUDPGenerator.C CustomUDPReceiver.C

bin_PROGRAMS = ska1_udpgen ska1_udprecv

AM_CXXFLAGS = -I. -I$(top_builddir)/src/Network -I/home/spip/linux_64/include

LDADD = libska1.la -L/home/dada/linux_64/lib $(top_builddir)/src/Network/libspipnet.la -lpsrdada

