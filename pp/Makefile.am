AM_CPPFLAGS = \
	-I ${top_srcdir}/libop \
	-I ${top_srcdir}/libutil \
	-I ${top_srcdir}/libdb \
	-I ${top_srcdir}/libopt++ \
	-I ${top_srcdir}/libutil++ \
	-I ${top_srcdir}/libop++ \
	-I ${top_srcdir}/libregex

noinst_LIBRARIES = libpp.a
libpp_a_SOURCES = \
	format_output.cpp \
	profile_container.cpp \
	sample_container_imp.cpp \
	counter_array.cpp \
	symbol_container_imp.cpp \
	derive_files.cpp \
	counter_util.cpp \
	counter_profile.cpp \
	profile.cpp \
	session.cpp \
	counter_array.h \
	profile.h \
	profile_container.h \
	counter_util.h \
	opp_symbol.h \
	format_output.h \
	counter_profile.h \
	derive_files.h \
	oprofpp_options.h \
	session.h \
	op_time_options.h \
	outsymbflag.h \
	symbol_container_imp.h \
	op_to_source_options.h \
	sample_container_imp.h \
	symbol_functors.h

bin_PROGRAMS = oprofpp op_to_source op_time op_merge

common_libs =  \
	libpp.a \
	../libopt++/libopt++.a \
	../libop++/libop++.a \
	../libregex/libop_regex.a \
	../libutil++/libutil++.a \
	../libop/libop.a \
	../libutil/libutil.a \
	../libdb/libdb.a


oprofpp_SOURCES = oprofpp.cpp oprofpp_options.cpp
oprofpp_LDADD = $(common_libs)

op_to_source_SOURCES = op_to_source.cpp op_to_source_options.cpp
op_to_source_LDADD = $(common_libs)

op_time_SOURCES = op_time.cpp op_time_options.cpp
op_time_LDADD = $(common_libs)

op_merge_SOURCES = op_merge.cpp
op_merge_LDADD = \
	libpp.a \
	../libopt++/libopt++.a \
	../libop++/libop++.a \
	../libutil++/libutil++.a \
	../libutil/libutil.a \
	../libdb/libdb.a
