all: sprofile.o

clean:
	rm -f *.o 
 
CFLAGS=-D__KERNEL__ -Wall -I/usr/src/linux/include -Wstrict-prototypes -O2 -fomit-frame-pointer -pipe -fno-strength-reduce -malign-loops=2 -malign-jumps=2 -malign-functions=2 -DMODULE -Wunused

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
