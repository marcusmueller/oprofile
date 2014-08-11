LIBS=@LIBERTY_LIBS@ @PFM_LIB@ @RT_LIB@
if BUILD_FOR_PERF_EVENT

AM_CPPFLAGS = \
	-I ${top_srcdir}/libutil \
	-I ${top_srcdir}/libutil++ \
	-I ${top_srcdir}/libop \
	-I ${top_srcdir}/libperf_events \
	-I ${top_srcdir}/libpe_utils \
	@PERF_EVENT_FLAGS@ \
	@OP_CPPFLAGS@

ocount_SOURCES = ocount.cpp \
	ocount_counter.h \
	ocount_counter.cpp


AM_CXXFLAGS = @OP_CXXFLAGS@
AM_LDFLAGS = @OP_LDFLAGS@

bin_PROGRAMS = ocount
ocount_LDADD = ../libpe_utils/libpe_utils.a \
	../libpe_utils/libpe_utils.a \
	../libop/libop.a \
	../libutil/libutil.a \
	../libutil++/libutil++.a

endif
