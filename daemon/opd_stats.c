/**
 * @file daemon/opd_stats.c
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

unsigned long opd_stats[OPD_MAX_STATS];

/**
 * opd_print_stats - print out latest statistics
 */
void opd_print_stats(void)
{
	printf("\n%s\n", op_get_time());
	printf("Nr. sample dumps: %lu\n", opd_stats[OPD_DUMP_COUNT]);
	printf("Nr. samples total: %lu\n", opd_stats[OPD_SAMPLES]);
	printf("Nr. kernel samples: %lu\n", opd_stats[OPD_KERNEL]);
	printf("Nr. lost samples (no kernel/user): %lu\n", opd_stats[OPD_NO_CTX]);
	printf("Nr. lost kernel samples: %lu\n", opd_stats[OPD_LOST_KERNEL]);
	printf("Nr. incomplete code structs: %lu\n", opd_stats[OPD_DANGLING_CODE]);
	fflush(stdout);
}
