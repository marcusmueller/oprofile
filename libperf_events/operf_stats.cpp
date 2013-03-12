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
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <iostream>
#include <errno.h>

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
	int total_lost_samples = 0;

	operf_log.append("/samples/operf.log");
	FILE * fp = fopen(operf_log.c_str(), "a");
	if (!fp) {
		fprintf(stderr, "Unable to open %s file.\n", operf_log.c_str());
		return;
	}
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
		fprintf(stderr, "Decrease the sampling rate to eliminate (or reduce) lost samples.\n");
	} else if (throttled) {
		fprintf(stderr, "* * * * WARNING: Profiling rate was throttled back by the kernel * * * *\n");
		fprintf(stderr, "The number of samples actually recorded is less than expected, but is\n");
		fprintf(stderr, "probably still statistically valid.  Decreasing the sampling rate is the\n");
		fprintf(stderr, "best option if you want to avoid throttling.\n");
	}

	// TODO: handle extended stats
	//operf_ext_print_stats();

	for (int i = OPERF_INDEX_OF_FIRST_LOST_STAT; i < OPERF_MAX_STATS; i++)
		total_lost_samples += operf_stats[i];

	if (total_lost_samples > (int)(OPERF_WARN_LOST_SAMPLES_THRESHOLD
				       * operf_stats[OPERF_SAMPLES]))
		fprintf(stderr, "\nSee the %s file for statistics about lost samples.\n", operf_log.c_str());

	fflush(fp);
	fclose(fp);
};

void operf_stats_recorder::write_throttled_event_files(std::vector< operf_event_t> const & events,
						       std::string const & stats_dir)
{
	string outputfile;
	ofstream outfile;
	string event_name;
	string throttled_dir;
	bool throttled_dir_created = false;
	int rc;

	throttled_dir =  stats_dir + "/throttled";

	for (unsigned index = 0; index < events.size(); index++) {
		if (events[index].throttled == true) {

			if (!throttled_dir_created) {
				rc = mkdir(throttled_dir.c_str(),
					   S_IRWXU | S_IRWXG
					   | S_IROTH | S_IXOTH);
				if (rc && (errno != EEXIST)) {
					cerr << "Error trying to create " << throttled_dir
					     << endl;
					perror("mkdir failed with");
					return;
				}
				throttled_dir_created = true;
			}

			/* Write file entry to indicate if the data sample was
			 * throttled.
			 */
			outputfile = throttled_dir + "/"
				+ events[index].name;

			outfile.open(outputfile.c_str());

			if (!outfile.is_open()) {
				cerr << "Internal error: Could not create " << outputfile
				     <<  strerror(errno) << endl;
			} else {
				outfile.close();
			}
		}
	  }
};


std::string  operf_stats_recorder::create_stats_dir(std::string const & cur_sampledir)
{
	int rc;
	int errno;
	std::string stats_dir;

	/* Assumption: cur_sampledir ends in slash */
	stats_dir =  cur_sampledir + "stats";
	rc = mkdir(stats_dir.c_str(),
		   S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

	if (rc && (errno != EEXIST)) {
		cerr << "Error trying to create stats dir. " << endl;
		perror("mkdir failed with");
		return NULL;
	}
	return stats_dir;
}
