AM_CPPFLAGS = \
	-I ${top_srcdir}/libutil \
	-I ${top_srcdir}/libop

AM_CFLAGS = @OP_CFLAGS@

check_PROGRAMS = parse_event_tests alloc_counter_tests

parse_event_tests_SOURCES = parse_event_tests.c
parse_event_tests_LDADD = ../libop.a ../../libutil/libutil.a

alloc_counter_tests_SOURCES = alloc_counter_tests.c
alloc_counter_tests_LDADD = ../libop.a ../../libutil/libutil.a


TESTS = parse_event_tests alloc_counter_tests
