AM_CPPFLAGS = \
	-I ${top_srcdir}/libop \
	-I ${top_srcdir}/libutil \
	-I ${top_srcdir}/libdb \
	-I ${top_srcdir}/libopt++ \
	-I ${top_srcdir}/libutil++ \
	-I ${top_srcdir}/libregex \
	-I ${top_srcdir}/libpp

AM_CXXFLAGS = @OP_CXXFLAGS@

bin_PROGRAMS = opreport opannotate opgprof

LIBS=@POPT_LIBS@ @BFD_LIBS@

pp_commons = common_option.cpp common_option.h

common_libs =  \
	../libpp/libpp.a \
	../libopt++/libopt++.a \
	../libregex/libop_regex.a \
	../libutil++/libutil++.a \
	../libop/libop.a \
	../libutil/libutil.a \
	../libdb/libodb.a

opreport_SOURCES = opreport.cpp \
	opreport_options.h opreport_options.cpp \
	populate.cpp populate.h $(pp_commons)
opreport_LDADD = $(common_libs)

opannotate_SOURCES = opannotate.cpp \
	opannotate_options.h opannotate_options.cpp \
	populate.cpp populate.h $(pp_commons)
opannotate_LDADD = $(common_libs)

opgprof_SOURCES = opgprof.cpp \
	opgprof_options.h opgprof_options.cpp \
	$(pp_commons)
opgprof_LDADD = $(common_libs)
