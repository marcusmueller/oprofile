LIBS=@POPT_LIBS@ @LIBERTY_LIBS@ @ELF_LIB@ @PFM_LIB@
if BUILD_FOR_PERF_EVENT

AM_CPPFLAGS = \
	-I ${top_srcdir}/libabi \
	-I ${top_srcdir}/libutil \
	-I ${top_srcdir}/libop \
	-I ${top_srcdir}/libopt++ \
	-I ${top_srcdir}/libutil++ \
	-I ${top_srcdir}/libdb \
	-I ${top_srcdir}/pe_profiling

# 	-I ${top_srcdir}/daemon

operf_SOURCES = operf.cpp \
	operf.h \
	operf_utils.cpp \
	operf_counter.cpp \
	operf_counter.h \
	operf_event.h \
	operf_process_info.h \
	operf_process_info.cpp \
	operf_sfile.cpp \
	operf_sfile.h \
	operf_kernel.h \
	operf_kernel.cpp \
	operf_mangling.h \
	operf_mangling.cpp

AM_CXXFLAGS = @OP_CXXFLAGS@
bin_PROGRAMS = operf
operf_LDADD =	../libopt++/libopt++.a \
	../libutil++/libutil++.a \
	../libdb/libodb.a \
	../libop/libop.a \
	../libutil/libutil.a \
	../libabi/libabi.a

#operf_LINK = $(CXX) $(AM_CXXFLAGS) $(AM_CPPFLAGS) $(CXXFLAGS) $(AM_LDFLAGS) $(LDFLAGS) -o $@

endif
