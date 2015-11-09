
lib_LTLIBRARIES = libska1.la

libska1_headers = spip/UDPFormatCustom.h

libska1_la_SOURCES = UDPFormatCustom.C

bin_PROGRAMS = ska1_udpgen ska1_udprecv ska1_udpdb

AM_CXXFLAGS = -I. -I$(top_builddir)/src/Network -I$(top_builddir)/src/Dada -I/home/spip/linux_64/include

LDADD = libska1.la -L/home/spip/linux_64/lib $(top_builddir)/src/Network/libspipnet.la $(top_builddir)/src/Dada/libspipdada.la -lpsrdada -lspead2 -L/opt/cross/lib -lboost_system

