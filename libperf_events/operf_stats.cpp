/**
 * @file libperf_events/operf_stats.cpp
 * Management of operf statistics
 *
 * @remark Copyright 2012 OProfile authors
 * @remark Read the file COPYING
 *
 * Created on: June 11, 2012
 * @author Maynard Johnson
 * (C) Copyright IBM Corp. 2012
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "operf_stats.h"
#include "op_get_time.h"

unsigned long operf_stats[OPERF_MAX_STATS];

/**
 * operf_print_stats - print out latest statistics to operf.log
 */
using namespace std;
void operf_print_stats(string sessiondir, char * starttime, bool throttled)
{
	string operf_log (sessiondir);
	operf_log.append("/samples/operf.log");
	FILE * fp = fopen(operf_log.c_str(), "a");
	fprintf(fp, "\nProfiling started at %s", starttime);
	fprintf(fp, "Profiling stopped at %s", op_get_time());
	fprintf(fp, "\n-- OProfile/operf Statistics --\n");
	fprintf(fp, "Nr. non-backtrace samples: %lu\n", operf_stats[OPERF_SAMPLES]);
	fprintf(fp, "Nr. kernel samples: %lu\n", operf_stats[OPERF_KERNEL]);
	fprintf(fp, "Nr. user space samples: %lu\n", operf_stats[OPERF_PROCESS]);
	fprintf(fp, "Nr. samples lost due to sample address not in expected range for domain: %lu\n",
	       operf_stats[OPERF_INVALID_CTX]);
	fprintf(fp, "Nr. lost kernel samples: %lu\n", operf_stats[OPERF_LOST_KERNEL]);
	fprintf(fp, "Nr. samples lost due to sample file open failure: %lu\n",
		operf_stats[OPERF_LOST_SAMPLEFILE]);
	fprintf(fp, "Nr. samples lost due to no permanent mapping: %lu\n",
		operf_stats[OPERF_LOST_NO_MAPPING]);
	fprintf(fp, "Nr. user context kernel samples lost due to no app info available: %lu\n",
	       operf_stats[OPERF_NO_APP_KERNEL_SAMPLE]);
	fprintf(fp, "Nr. user samples lost due to no app info available: %lu\n",
	       operf_stats[OPERF_NO_APP_USER_SAMPLE]);
	fprintf(fp, "Nr. backtraces skipped due to no file mapping: %lu\n",
	       operf_stats[OPERF_BT_LOST_NO_MAPPING]);
	fprintf(fp, "Nr. hypervisor samples dropped due to address out-of-range: %lu\n",
	       operf_stats[OPERF_LOST_INVALID_HYPERV_ADDR]);
	fprintf(fp, "Nr. samples lost reported by perf_events kernel: %lu\n",
	       operf_stats[OPERF_RECORD_LOST_SAMPLE]);

	if (operf_stats[OPERF_RECORD_LOST_SAMPLE]) {
		fprintf(stderr, "\n\n * * * ATTENTION: The kernel lost %lu samples. * * *\n",
		        operf_stats[OPERF_RECORD_LOST_SAMPLE]);
	} else if (throttled) {
		fprintf(stderr, "* * * * WARNING: Profiling rate was throttled back by the kernel * * * *\n");
		fprintf(stderr, "The number of samples actually recorded is less than expected.\n");
	}
	if (operf_stats[OPERF_RECORD_LOST_SAMPLE] || throttled) {
		fprintf(stderr, "Try decreasing your sampling rate.\n");
		fprintf(stderr, "See the %s file for more profiling statistics.\n", operf_log.c_str());
	}
	// TODO: handle extended stats
	//operf_ext_print_stats();

	fflush(fp);
}
