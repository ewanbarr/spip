
lib_LTLIBRARIES = libska1.la

libska1_headers = spip/CustomUDPGenerator.h 

libska1_la_SOURCES = CustomUDPGenerator.C

bin_PROGRAMS = ska1_udpgen

AM_CXXFLAGS = -I. -I$(top_builddir)/src/Network -I/home/spip/linux_64/include

LDADD = libska1.la -L/home/dada/linux_64/lib $(top_builddir)/src/Network/libspipnet.la -lpsrdada

