
noinst_LTLIBRARIES = libspipdsp.la

libspipdsp_headers = spip/Container.h \
	spip/ContainerRAM.h \
	spip/ContainerRing.h \
	spip/DelayPipeline.h \
	spip/FractionalDelay.h \
	spip/IntegerDelay.h \
	spip/Transformation.h

libspipdsp_la_SOURCES = Container.C \
	ContainerRAM.C \
	ContainerRing.C \
	DelayPipeline.C \
	FractionalDelay.C \
	IntegerDelay.C

bin_PROGRAMS = delay_pipeline

delay_pipeline_SOURCES = delay_pipeline.C

AM_CXXFLAGS = @PSRDADA_CFLAGS@ \
	-I$(top_builddir)/src/Affinity\
	-I$(top_builddir)/src/Util \
	-I$(top_builddir)/src/Dada \
  @CUDA_CFLAGS@

LDADD = libspipdsp.la \
	$(top_builddir)/src/Affinity/libspipaffinity.la \
	$(top_builddir)/src/Dada/libspipdada.la \
	$(top_builddir)/src/Util/libspiputil.la \
	@PSRDADA_LIBS@

