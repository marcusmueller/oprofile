AM_CPPFLAGS = \
	-I ${top_srcdir}/libutil++

COMMON_LIBS = ../../libutil/libutil.a ../libutil++.a

AM_CXXFLAGS = @OP_CXXFLAGS@

check_PROGRAMS = \
	string_manip_tests \
	string_filter_tests \
	comma_list_tests

string_manip_tests_SOURCES = string_manip_tests.cpp
string_manip_tests_LDADD = ${COMMON_LIBS}

string_filter_tests_SOURCES = string_filter_tests.cpp
string_filter_tests_LDADD = ${COMMON_LIBS}

comma_list_tests_SOURCES = comma_list_tests.cpp
comma_list_tests_LDADD = ${COMMON_LIBS}

TESTS = ${check_PROGRAMS}
