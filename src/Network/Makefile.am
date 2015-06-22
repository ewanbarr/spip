
lib_LTLIBRARIES = libspipnet.la

libspipnet_headers = spip/Socket.h spip/UDPSocket.h spip/UDPSocketReceive.h spip/UDPSocketSend.h \
										 spip/UDPGenerator.h spip/UDPReceiver.h spip/UDPStats.h

libspipnet_la_SOURCES = Socket.C UDPSocket.C UDPSocketReceive.C UDPSocketSend.C UDPGenerator.C \
												UDPReceiver.C UDPStats.C

AM_CXXFLAGS = -I/home/spip/linux_64/include

LDFLAGS += -lnuma

