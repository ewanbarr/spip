
lib_LTLIBRARIES = libska1.la

libska1_headers = spip/UDPFormatCustom.h

libska1_la_SOURCES = UDPFormatCustom.C

bin_PROGRAMS = ska1_udpgen ska1_udprecv ska1_udpdb

AM_CXXFLAGS = -I. -I$(top_builddir)/src/Network -I$(top_builddir)/src/Dada

LDADD = libska1.la $(top_builddir)/src/Network/libspipnet.la $(top_builddir)/src/Dada/libspipdada.la -L/opt/cross/lib 

if HAVE_PSRDADA

AM_CXXFLAGS += @PSRDADA_CFLAGS@

LDADD += @PSRDADA_LIBS@

endif
