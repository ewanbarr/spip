
noinst_LTLIBRARIES = libspipnet.la

libspipnet_headers = spip/Socket.h spip/UDPSocket.h spip/UDPSocketReceive.h spip/UDPSocketSend.h \
                     spip/UDPGenerator.h spip/UDPReceiver.h spip/UDPStats.h \
                     spip/UDPFormat.h spip/TCPSocket.h spip/TCPSocketServer.h

libspipnet_la_SOURCES = Socket.C C UDPSocket.C UDPSocketReceive.C UDPSocketSend.C \
                        UDPGenerator.C UDPReceiver.C UDPStats.C \
                        UDPFormat.C TCPSocket.C TCPSocketServer.C

AM_CXXFLAGS = -I$(top_builddir)/src/Affinity \
							-I$(top_builddir)/src/Util \
  						-I/opt/cross/include -std=c++0x

LDADD = $(top_builddir)/src/Affinity/libspipaffinity.la \
			  $(top_builddir)/src/Util/libspiputil.la

if HAVE_PSRDADA

libspipnet_headers += spip/UDPReceiveDB.h spip/UDPReceiveMergeDB.h

libspipnet_la_SOURCES += UDPReceiveDB.C UDPReceiveMergeDB.C

AM_CXXFLAGS += @PSRDADA_CFLAGS@ -I$(top_builddir)/src/Dada
LDADD += $(top_builddir)/src/Dada/libspipdada.la

endif


if HAVE_SPEAD2

libspipnet_la_SOURCES += SPEADReceiveDB.C SPEADReceiver.C SPEADBeamFormerConfig.C \
                         SPEADReceiverMerge.C SPEADReceiveMergeDB.C

AM_CXXFLAGS += @BOOST_CPPFLAGS@ @SPEAD2_CFLAGS@
LDADD += @BOOST_SYSTEM_LDFLAGS@ @SPEAD2_LIBS@

endif

if HAVE_CUDA

AM_CXXFLAGS += @CUDA_CFLAGS@
LDADD += @CUDA_LIBS@

endif
