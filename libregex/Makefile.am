# FIXME trouble with BINDIR, cause I need to locate stl.pat data file
AM_CPPFLAGS=-I ${top_srcdir}/libutil++ -DBINDIR=\"$(bindir)\"

noinst_LIBRARIES = libop_regex.a

libop_regex_a_SOURCES = op_regex.cpp op_regex.h demangle_symbol.h \
	demangle_symbol.cpp

noinst_PROGRAMS = regex_test

regex_test_SOURCES = regex_test.cpp
regex_test_LDADD = libop_regex.a ../libutil++/libutil++.a

# FIXME is this right for both make install and make dist
# FIXME pp tools will find stl.pat thrugh a ugly -DBINDIR= above ...
patterndir = ${prefix}/bin
pattern_DATA = stl.pat

EXTRA_DIST = mangled-name.txt
