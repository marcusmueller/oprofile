dist_sources = \
	abi.cpp \
	op_abi.cpp \
	abi.h \
	op_abi.h \
	op_import.cpp \
	abi_test.cpp

EXTRA_DIST = $(dist_sources)

LIBS=@POPT_LIBS@ @LIBERTY_LIBS@

if enable_abi

AM_CPPFLAGS = \
	-I ${top_srcdir}/libop \
	-I ${top_srcdir}/libutil \
	-I ${top_srcdir}/libdb \
	-I ${top_srcdir}/libopt++

AM_CXXFLAGS = @OP_CXXFLAGS@

noinst_LIBRARIES = libabi.a
libabi_a_SOURCES = abi.cpp op_abi.cpp

bin_PROGRAMS = op_import

op_import_SOURCES = op_import.cpp
op_import_LDADD = \
	libabi.a \
	../libdb/libodb.a \
	../libopt++/libopt++.a \
	../libutil++/libutil++.a \
	../libutil/libutil.a

check_PROGRAMS = abi_test

abi_test_SOURCES = abi_test.cpp
abi_test_LDADD = \
	libabi.a \
	../libdb/libodb.a \
	../libopt++/libopt++.a \
	../libutil++/libutil++.a \
	../libutil/libutil.a

endif
