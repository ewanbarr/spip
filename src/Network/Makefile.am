
lib_LTLIBRARIES = libspipnet.la

libspipnet_headers = spip/Socket.h spip/UDPSocket.h spip/UDPSocketReceive.h spip/UDPSocketSend.h \
                     spip/UDPGenerator.h spip/UDPReceiver.h spip/UDPStats.h \
                     spip/UDPFormat.h spip/TCPSocket.h spip/TCPSocketServer.h

libspipnet_la_SOURCES = Socket.C C UDPSocket.C UDPSocketReceive.C UDPSocketSend.C \
                        UDPGenerator.C UDPReceiver.C UDPStats.C \
                        UDPFormat.C TCPSocket.C TCPSocketServer.C

AM_CXXFLAGS = -I$(top_builddir)/src/Affinity \
							-I$(top_builddir)/src/Util \
  						-I/opt/cross/include -std=c++0x

if HAVE_PSRDADA

libspipnet_headers += spip/UDPReceiveDB.h

libspipnet_la_SOURCES += UDPReceiveDB.C

AM_CXXFLAGS += @PSRDADA_CFLAGS@ -I$(top_builddir)/src/Dada

endif


if HAVE_SPEAD2

libspipnet_la_SOURCES += SPEADReceiveDB.C SPEADReceiver.C SPEADBeamFormerConfig.C \
                         SPEADReceiverMerge.C SPEADReceiveMergeDB.C

AM_CXXFLAGS += @SPEAD2_CFLAGS@

endif

