LIBS=@POPT_LIBS@ @LIBERTY_LIBS@ @ELF_LIB@ @PFM_LIB@
if BUILD_FOR_PERF_EVENT

AM_CPPFLAGS = \
	-I ${top_srcdir}/libutil \
	-I ${top_srcdir}/libop \
	-I ${top_srcdir}/libopt++ \
	-I ${top_srcdir}/libutil++ \
	-I ${top_srcdir}/libperf_events

operf_SOURCES = operf.cpp

AM_CXXFLAGS = @OP_CXXFLAGS@
bin_PROGRAMS = operf
operf_LDADD =	../libopt++/libopt++.a \
	../libperf_events/libperf_events.a \
	../libutil++/libutil++.a \
	../libdb/libodb.a \
	../libop/libop.a \
	../libutil/libutil.a \
	../libabi/libabi.a

endif
