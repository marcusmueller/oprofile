# Generated automatically from Makefile.in by configure.
AR=ar

CFLAGS=-Wall -Wstrict-prototypes -pipe -Wunused -W -Wshadow -Wmissing-prototypes -Winline -g -O2
CFLAGS+=-DNDEBUG

.PHONY: clean

all: libdb.a db-test

clean:
	rm -rf *.o *~ .deps db-test libdb.a

libdb.a: db-insert.o db-manage.o db-travel.o db-debug.o
	rm -f $@
	$(AR) cr $@ $^

db-test: db-test.o libdb.a
	$(CC) $(CFLAGS) $^ -o $@

ALL_SOURCES = db-insert.c db-manage.c db-travel.c db-debug.c db-test.c

include ../Rules.make
