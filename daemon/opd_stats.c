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
#include "oprofiled.h"

#include "op_get_time.h"

#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>

unsigned long opd_stats[OPD_MAX_STATS];

/**
 * opd_print_stats - print out latest statistics
 */
void opd_print_stats(void)
{
	DIR * dir;
	struct dirent * dirent;

	printf("\n%s\n", op_get_time());
	printf("Nr. sample dumps: %lu\n", opd_stats[OPD_DUMP_COUNT]);
	printf("Nr. samples total: %lu\n", opd_stats[OPD_SAMPLES]);
	printf("Nr. kernel samples: %lu\n", opd_stats[OPD_KERNEL]);
	printf("Nr. lost samples (no kernel/user): %lu\n", opd_stats[OPD_NO_CTX]);
	printf("Nr. lost kernel samples: %lu\n", opd_stats[OPD_LOST_KERNEL]);
	printf("Nr. samples lost due to sample file open failure: %lu\n",
		opd_stats[OPD_LOST_SAMPLEFILE]);
	printf("Nr. incomplete code structs: %lu\n", opd_stats[OPD_DANGLING_CODE]);
	printf("Nr. event lost due to buffer overflow: %u\n",
	       opd_read_fs_int("/dev/oprofile/stats", "event_lost_overflow", 0));
	printf("Nr. samples without file mapping: %u\n",
	       opd_read_fs_int("/dev/oprofile/stats", "sample_lost_no_mapping", 0));
	printf("Nr. samples without mm: %u\n",
	       opd_read_fs_int("/dev/oprofile/stats", "sample_lost_no_mm", 0));

	if (!(dir = opendir("/dev/oprofile/stats/")))
		return;
	while ((dirent = readdir(dir))) {
		int cpu_nr;
		char filename[256];
		if (sscanf(dirent->d_name, "cpu%d", &cpu_nr) != 1)
			continue;
		snprintf(filename, 256,
			 "/dev/oprofile/stats/%s", dirent->d_name);
		printf("Nr. samples lost cpu buffer overflow: %u\n",
		       opd_read_fs_int(filename, "sample_lost_overflow", 0));
		printf("Nr. samples lost task exit: %u\n",
		       opd_read_fs_int(filename, "sample_lost_task_exit", 0));
		printf("Nr. samples received: %u\n",
		       opd_read_fs_int(filename, "sample_received", 0));
	}
	closedir(dir);
	fflush(stdout);
}
