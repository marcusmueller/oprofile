all: sprofile.o

clean:
	rm -f *.o 
 
# FIXME: move NMI code into separate to allow -march=ppro for that
CFLAGS=-D__KERNEL__ -I/usr/src/linux/include -Wall -Wstrict-prototypes -O2 -fomit-frame-pointer -pipe  -mpreferred-stack-boundary=2 -march=i586 -fno-strict-aliasing -DMODULE -Wunused
 
ASMFLAGS=-D__ASSEMBLY__ -D__KERNEL__ -traditional
 
sprofile.o: sprofile_c.o sprofile_nmi.o sprofile_k.o cp_events.o 
	ld -r -o $(G) $@ sprofile_c.o sprofile_nmi.o sprofile_k.o cp_events.o

sprofile_c.o: sprofile.c sprofile.h
	gcc $(CFLAGS) $(G) -c -o $@ $< 

sprofile_k.o: sprofile_k.c sprofile.h
	gcc $(CFLAGS) $(G) -c -o $@ $< 

sprofile.s: sprofile.c sprofile.h
	gcc $(CFLAGS) $(G) -S $<
 
cp_events.o: cp_events.c
	gcc $(CFLAGS) $(G) -c -o $@ $< 
 
sprofile_nmi.o: sprofile_nmi.S
	gcc $(ASMFLAGS) -c -o $@ $<
