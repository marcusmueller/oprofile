/**
 * @file opd_stats.c
 * Management of daemon statistics
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 */

#include "opd_stats.h"

#include "op_get_time.h"

#include <stdlib.h>
#include <stdio.h>

extern int nr_images;

unsigned long opd_stats[OPD_MAX_STATS];

/**
 * opd_print_stats - print out latest statistics
 */
void opd_print_stats(void)
{
	printf("\n%s\n", op_get_time());
	printf("Nr. image struct: %d\n", nr_images);
	printf("Nr. kernel samples: %lu\n", opd_stats[OPD_KERNEL]);
	printf("Nr. modules samples: %lu\n", opd_stats[OPD_MODULE]);
	printf("Nr. modules samples lost: %lu\n", opd_stats[OPD_LOST_MODULE]);
	printf("Nr. lost userspace samples: %lu\n", opd_stats[OPD_NO_MM]);
	printf("Nr. sample dumps: %lu\n", opd_stats[OPD_DUMP_COUNT]);
	printf("Nr. samples total: %lu\n", opd_stats[OPD_SAMPLES]);
	printf("Nr. individual samples total: %lu\n", opd_stats[OPD_SAMPLE_COUNTS]); 
	fflush(stdout);
}
