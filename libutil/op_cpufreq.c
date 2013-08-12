/**
 * @file op_cpufreq.c
 * get cpu frequency definition
 *
 * @remark Copyright 2011 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 * @author Suravee Suthikulpanit
 */

#include <stdio.h>
#include <stdlib.h>

#include "op_fileio.h"

/*
 * Use frequency information from /proc/cpuinfo. This should be available in most case.
 * However, this number is the current cpu frequency which could be in the idle state.
 * We should actually report the max frequency because the cpu should switch into the
 * highest power state when running.
 */
static double op_cpu_freq_cpuinfo(void)
{
	double fval = 0.0;
	unsigned long uval;
	char * line = NULL;

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
			break;
		/* ppc/ppc64 */
		if (sscanf(line, "clock : %lfMHz", &fval) == 1)
			break;
		/* alpha */
		if (sscanf(line, "cycle frequency [Hz] : %lu", &uval) == 1) {
			fval = uval / 1E6;
			break;
		}
		/* sparc64 if CONFIG_SMP only */
		if (sscanf(line, "Cpu0ClkTck : %lx", &uval) == 1) {
			fval = uval / 1E6;
			break;
		}
		/* mips including loongson2 */
		if (sscanf(line, "BogoMIPS		: %lu", &uval) == 1) {
			fval = uval * 3 / 2;
			break;
		}
		/* s390 doesn't provide cpu freq, checked up to 2.6-test4 */

		free(line);
	}

	if (line)
		free(line);
	op_close_file(fp);

	return fval;
}


/*
 * Use frequency information from /sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq
 * This number is the maximum cpu frequency of the processor.
 */
static double op_cpu_freq_sys_devices(void)
{
	double fval = 0.0;
	char * line = NULL;

	FILE * fp = op_try_open_file("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq", "r");
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

		if (sscanf(line, "%lf", &fval) == 1)
			break;

		free(line);
	}

	if (line)
		free(line);
	op_close_file(fp);

        /* Return the frequency in MHz.  When the frequency is
         * printed it is assumed to be in units of MHz.
	 */
	return fval/1000;
}


double op_cpu_frequency(void)
{
	/*
	 * Typically, system changes the frequency scaling of the cpus
	 * based on the workload.  /proc/cpuinfo reports the current
	 * frequency value which might not be at the maximum at the time
	 * of query.  Since cpu switch into running state with max frequency
	 * when under workload, we should make sure to report the max frequency
	 * value acquiring from /sys/devices when available.
	 */

	double freq = op_cpu_freq_sys_devices();
	if (freq == 0) {
		freq = op_cpu_freq_cpuinfo();
	}
	return freq;
}
