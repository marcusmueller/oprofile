#include <stdio.h>

int main (int argc, char *argv[])
{
	unsigned long long a,b;

	sscanf(argv[1],"%Lu",&a);
	sscanf(argv[2],"%Lu",&b);
	printf("%Lu",b-a);
	return 0;
}
