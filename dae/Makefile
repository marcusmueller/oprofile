all: cprofiled cp_help rdtsc div min loop
 
.PHONY: all clean srcdoc 
clean:
	rm -f cprofiled cp_help rdtsc div min loop *.o
 
CC=gcc 
CFLAGS=-Wall -Wstrict-prototypes -O2 -pipe -Wstrict-prototypes -Wunused -W -Wshadow -Wmissing-prototypes -Winline
LIBS=-lpopt
INCLUDES= 
HEADERS=cprofiled.h
SOURCES=cprofiled.c cpd_util.c cpd_proc.c cp_events.c
 
srcdoc: $(SOURCES)
	kernel-doc -html $(SOURCES) >cprofiled-src.html
 
cprofiled: $(SOURCES) $(HEADERS) 
	$(CC) $(CFLAGS) $(INCLUDES) -g -o $@ $(SOURCES) $(LIBS)

cp_help: cp_events.c
	$(CC) $(CFLAGS) -DCP_EVENTS_DESC -DCP_EVENTS_MAIN $(INCLUDES) -g -o cp_help cp_events.c

rdtsc: rdtsc.c
	$(CC) $(CFLAGS) $(INCLUDES) -o rdtsc rdtsc.c 

div: div.c 
	$(CC) $(CFLAGS) $(INCLUDES) -o div div.c
 
min: min.c 
	$(CC) $(CFLAGS) $(INCLUDES) -o min min.c

loop:loop.c
	$(CC) -o loop loop.c
