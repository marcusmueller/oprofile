AM_CPPFLAGS = \
	-I ${top_srcdir}/libop \
	-I ${top_srcdir}/libutil \
	-I ${top_srcdir}/libdb

AM_CFLAGS = @OP_CFLAGS@

noinst_PROGRAMS = db_test

db_test_SOURCES = db_test.c
db_test_LDADD = ../libodb.a

TESTS = db_test