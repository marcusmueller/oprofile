# $Id: Makefile,v 1.5 2000/08/01 21:10:05 moz Exp $

all: cprofiled cp_help
 
.PHONY: all clean srcdoc 
clean:
	rm -f cprofiled cp_help *.o && $(MAKE) -C pp clean
 
CC=gcc 
CFLAGS=-Wall -Wstrict-prototypes -O2 -pipe -Wstrict-prototypes -Wunused -W -Wshadow -Wmissing-prototypes -Winline
LIBS=-lpopt
INCLUDES= 
HEADERS=cprofiled.h
SOURCES=cprofiled.c cpd_util.c cpd_proc.c ../cp_events.c
 
srcdoc: cprofiled-src.html

cprofiled-src.html: $(SOURCES)
	kernel-doc -html $(SOURCES) >cprofiled-src.html
 
cprofiled: $(SOURCES) $(HEADERS) 
	rm -f ../cp_events.o 
	$(CC) $(CFLAGS) $(INCLUDES) -g -o $@ $(SOURCES) $(LIBS)

cp_help: ../cp_events.c
	rm -f ../cp_events.o
	$(CC) $(CFLAGS) -DCP_EVENTS_DESC -DCP_EVENTS_MAIN $(INCLUDES) -g -o cp_help ../cp_events.c
