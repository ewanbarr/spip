
lib_LTLIBRARIES = libspipnet.la

libspipnet_headers = spip/Socket.h spip/UDPSocket.h spip/UDPSocketReceive.h spip/UDPSocketSend.h \
										 spip/UDPGenerator.h spip/UDPReceiver.h spip/UDPStats.h spip UDPReceiveDB.h \
									   spip/UDPFormat.h spip/TCPSocket.h spip/TCPSocketServer.h

libspipnet_la_SOURCES = Socket.C UDPSocket.C UDPSocketReceive.C UDPSocketSend.C UDPGenerator.C \
												UDPReceiver.C UDPStats.C UDPReceiveDB.C UDPFormat.C \
												TCPSocket.C TCPSocketServer.C

AM_CXXFLAGS = -I/home/spip/linux_64/include -I$(top_builddir)/src/Dada

AM_LDFLAGS = -lnuma

