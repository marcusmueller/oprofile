if BUILD_FOR_PERF_EVENT

AM_CPPFLAGS = \
	-I ${top_srcdir}/libutil \
	-I ${top_srcdir}/libutil++ \
	-I ${top_srcdir}/libop \
	-I ${top_srcdir}/libpe_utils \
	-I ${top_srcdir}/libperf_events \
	@PERF_EVENT_FLAGS@ \
	@OP_CPPFLAGS@

AM_CXXFLAGS = @OP_CXXFLAGS@

noinst_LIBRARIES = libpe_utils.a
libpe_utils_a_SOURCES =  \
	op_pe_utils.h \
	op_pe_utils.cpp


endif
