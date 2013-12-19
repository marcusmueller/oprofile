if BUILD_FOR_PERF_EVENT

AM_CPPFLAGS = \
	-I ${top_srcdir}/libabi \
	-I ${top_srcdir}/libutil \
	-I ${top_srcdir}/libutil++ \
	-I ${top_srcdir}/libop \
	-I ${top_srcdir}/libdb \
	-I ${top_srcdir}/libperf_events \
	-I ${top_srcdir}/libpe_utils \
	@PERF_EVENT_FLAGS@ \
	@OP_CPPFLAGS@

AM_CXXFLAGS = @OP_CXXFLAGS@

noinst_LIBRARIES = libperf_events.a
libperf_events_a_SOURCES =  \
	operf_utils.h \
	operf_utils.cpp \
	operf_event.h \
	operf_counter.h \
	operf_counter.cpp \
	operf_process_info.h \
	operf_process_info.cpp \
	operf_kernel.cpp \
	operf_kernel.h \
	operf_mangling.cpp \
	operf_mangling.h \
	operf_sfile.cpp \
	operf_sfile.h \
	operf_stats.cpp \
	operf_stats.h

endif
