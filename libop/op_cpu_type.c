/**
 * @file op_cpu_type.c
 * CPU type determination
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon <moz@compsoc.man.ac.uk>
 * @author Philippe Elie <phil_el@wanadoo.fr>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "op_cpu_type.h"

/**
 * op_get_cpu_type - get from /proc/sys/dev/oprofile/cpu_type the cpu type
 *
 * returns %CPU_NO_GOOD if the CPU could not be identified
 */
op_cpu op_get_cpu_type(void)
{
	int cpu_type = CPU_NO_GOOD;
	char str[10];

	FILE * fp;

	fp = fopen("/proc/sys/dev/oprofile/cpu_type", "r");
	if (!fp) {
		fprintf(stderr, "Unable to open /proc/sys/dev/oprofile/cpu_type for reading\n");
		return cpu_type;
	}

	fgets(str, 9, fp);

	sscanf(str, "%d\n", &cpu_type);

	fclose(fp);

	return cpu_type;
}

 
static char const * cpu_names[MAX_CPU_TYPE] = {
	"Pentium Pro",
	"PII",
	"PIII",
	"Athlon",
	"CPU with RTC device"
};
 

/**
 * op_get_cpu_type_str - get the cpu string.
 * @param cpu_type  the cpu type identifier
 *
 * The function always return a valid char const *
 * the core cpu denomination or "invalid cpu type" if
 * @cpu_type is not valid.
 */
char const * op_get_cpu_type_str(op_cpu cpu_type)
{
	if (cpu_type < 0 || cpu_type >= MAX_CPU_TYPE) {
		return "invalid cpu type";
	}

	return cpu_names[cpu_type];
}

static int cpu_nr_counters[MAX_CPU_TYPE] = {
	2, /* PPro */
	2, /* PII */
	2, /* PIII */
	4, /* Athlon */
	1  /* RTC */
};

/**
 * compute the number of counters available
 * @param cpu_type numeric processor type
 *
 * returns 0 if the CPU could not be identified
 */
int op_get_nr_counters(op_cpu cpu_type)
{
	if (cpu_type < 0 || cpu_type > MAX_CPU_TYPE)
		return 0;

	return cpu_nr_counters[cpu_type];
}
