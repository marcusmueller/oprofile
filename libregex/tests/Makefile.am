AM_CPPFLAGS = \
	-I ${top_srcdir}/libutil++ \
	-I ${top_srcdir}/libregex

AM_CXXFLAGS = @OP_CXXFLAGS@

noinst_PROGRAMS = regex_test

regex_test_SOURCES = regex_test.cpp
regex_test_LDADD = \
	../libop_regex.a \
	../../libutil++/libutil++.a

EXTRA_DIST = mangled-name.txt

# FIXME: add a script which do that and use TESTS = script
TESTS = regex_test
