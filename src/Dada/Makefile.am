
if HAVE_PSRDADA

lib_LTLIBRARIES = libspipdada.la

libspipdada_headers = spip/DataBlock.h spip/DataBlockRead.h spip/DataBlockWrite.h spip/DataBlockView.h spip/DataBlockStats.h spip/BlockFormat.h

libspipdada_la_SOURCES = DataBlock.C DataBlockRead.C DataBlockWrite.C DataBlockView.C DataBlockStats.C BlockFormat.C

AM_CXXFLAGS = @PSRDADA_CFLAGS@ -I$(top_builddir)/src/Network

LDADD = $(top_src_dir)/src/Netwrok/libspipnet.la

endif
