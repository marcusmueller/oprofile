LIBS=@LIBERTY_LIBS@ @PFM_LIB@
if BUILD_FOR_PERF_EVENT

AM_CPPFLAGS = \
	-I ${top_srcdir}/libutil \
	-I ${top_srcdir}/libop \
	-I ${top_srcdir}/libutil++ \
	-I ${top_srcdir}/libperf_events \
	-I ${top_srcdir}/libpe_utils \
	@PERF_EVENT_FLAGS@ \
	@OP_CPPFLAGS@

operf_SOURCES = operf.cpp

AM_CXXFLAGS = @OP_CXXFLAGS@
AM_LDFLAGS = @OP_LDFLAGS@

bin_PROGRAMS = operf
operf_LDADD = ../libperf_events/libperf_events.a \
	../libpe_utils/libpe_utils.a \
	../libutil++/libutil++.a \
	../libdb/libodb.a \
	../libop/libop.a \
	../libutil/libutil.a \
	../libabi/libabi.a

endif
