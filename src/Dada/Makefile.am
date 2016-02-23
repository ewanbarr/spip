
noinst_LTLIBRARIES = libspipdada.la

libspipdada_headers = 

libspipdada_la_SOURCES = 

if HAVE_PSRDADA

libspipdada_headers += \
	spip/DataBlock.h \
	spip/DataBlockRead.h \
	spip/DataBlockWrite.h \
	spip/DataBlockView.h \
	spip/DataBlockStats.h

libspipdada_la_SOURCES += \
	DataBlock.C \
 	DataBlockRead.C \
	DataBlockWrite.C \
	DataBlockView.C \
	DataBlockStats.C

AM_CXXFLAGS = \
	@PSRDADA_CFLAGS@ \
	-I$(top_builddir)/src/Network \
	-I$(top_builddir)/src/Util
 
endif
