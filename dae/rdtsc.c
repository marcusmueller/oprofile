#include <stdio.h>

#define rdtsc(val1,val2) \
       __asm__ __volatile__("rdtsc" \
                           : "=a" (val1), "=d" (val2) \
                           : /* */)
int main (int argc, char *argv[])
{
	unsigned long tsc[2];

	rdtsc(tsc[0],tsc[1]);
	
	printf("%Lu\n", *((unsigned long long *)tsc));
	return 0; 
}
