AM_CPPFLAGS=-I ${top_srcdir}/libutil++

noinst_LIBRARIES = libop_regex.a

libop_regex_a_SOURCES = op_regex.cpp op_regex.h demangle_symbol.h \
	demangle_symbol.cpp

noinst_PROGRAMS = regex_test

regex_test_SOURCES = regex_test.cpp
regex_test_LDADD = libop_regex.a ../libutil++/libutil++.a

datadir = $(prefix)/share/oprofile
dist_data_DATA = stl.pat

EXTRA_DIST = mangled-name.txt
