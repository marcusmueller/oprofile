#include <stdio.h>

int main (int argc, char *argv[])
{
	unsigned long long a,b;

	sscanf(argv[1],"%Lu",&a);
	sscanf(argv[2],"%Lu",&b);
	printf("%Lf",((long double)a)/((long double)b));
	return 0;
}
