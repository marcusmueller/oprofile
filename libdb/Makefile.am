AM_CPPFLAGS = \
	-I ${top_srcdir}/libop \
	-I ${top_srcdir}/libutil

noinst_LIBRARIES = libdb.a

libdb_a_SOURCES = \
	db_manage.c \
	db_insert.c \
	db_travel.c \
	db_debug.c \
	db_stat.c \
	db_hash.h

noinst_PROGRAMS = db_test

db_test_SOURCES = db_test.c
db_test_LDADD = libdb.a
