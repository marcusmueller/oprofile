/**
 * @file op_cpu_type.c
 * CPU type determination
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "op_cpu_type.h"

/**
 * op_get_cpu_type - get the cpu type from the kernel
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
		/* Hmm. Not there. Try 2.5's oprofilefs one instead. */
		fp = fopen("/dev/oprofile/cpu_type", "r");
		if (!fp) {
			fprintf(stderr, "Unable to open cpu_type file for reading\n");
			return cpu_type;
		}
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
	"CPU with timer interrupt",
	"CPU with RTC device",
	"P4 / Xeon",
	"IA64",
	"Itanium",
	"Itanium 2",
	"Hammer"
};
 

/**
 * op_get_cpu_type_str - get the cpu string.
 * @param cpu_type  the cpu type identifier
 *
 * The function always return a valid char const *
 * the core cpu denomination or "invalid cpu type" if
 * cpu_type is not valid.
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
	1, /* Timer interrupt */
	1, /* RTC */
	8, /* P4 / Xeon */
	4, /* IA64 */
	4, /* IA64 (Merced) */
	4, /* IA64 (McKinley) */
	4  /* Hammer */
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
