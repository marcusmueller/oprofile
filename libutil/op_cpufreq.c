/**
 * @file op_cpufreq.c
 * get cpu frequency definition
 *
 * @remark Copyright 2003 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 */

#include <stdio.h>
#include <stdlib.h>

#include "op_fileio.h"

double op_cpu_frequency(void)
{
	double fval;
	unsigned long uval;
	char * line;

	FILE * fp = op_try_open_file("/proc/cpuinfo", "r");
	if (!fp)
		return 0.0;

	while (1) {
		line = op_get_line(fp);

		if (!line)
			break;

		if (line[0] == '\0') {
			free(line);
			continue;
		}

		/* x86/parisc/ia64/x86_64 */
		if (sscanf(line, "cpu MHz : %lf", &fval) == 1)
			return fval;
		/* ppc/ppc64 */
		if (sscanf(line, "clock : %lfMHz", &fval) == 1)
			return fval;
		/* alpha */
		if (sscanf(line, "cycle frequency [Hz] : %lu", &uval) == 1)
			return uval / 1E6;
		/* sparc64 if CONFIG_SMP only */
		if (sscanf(line, "Cpu0ClkTck : %lx", &uval) == 1)
			return uval / 1E6;

		free(line);
	}

	op_close_file(fp);

	return 0.0;
}
