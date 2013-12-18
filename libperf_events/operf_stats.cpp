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

static string create_stats_dir(string const & cur_sampledir);
static void write_throttled_event_files(vector< operf_event_t> const & events,
                                        string const & stats_dir);

static void _write_stats_file(string const & stats_filename, unsigned long lost_sample_count)
{
	ofstream stats_file(stats_filename.c_str(), ios_base::out);
	if (stats_file.good()) {
		stats_file << lost_sample_count;
		stats_file.close();
	} else {
		cerr << "Unable to write to stats file " << stats_filename << endl;
	}
}

void operf_print_stats(string sessiondir, char * starttime, bool throttled,
                       vector< operf_event_t> const & events)
{
	string operf_log (sessiondir);
	unsigned long total_lost_samples = 0;
	bool stats_dir_valid = true;

	string stats_dir = create_stats_dir(sessiondir + "/" + "samples/current/");
	if (strcmp(stats_dir.c_str(), "") != 0) {
		// If there are throttled events print them
		if (throttled)
			write_throttled_event_files(events, stats_dir);
	} else {
		stats_dir_valid = false;
		perror("Unable to create stats dir");
	}

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

	for (int i = OPERF_INDEX_OF_FIRST_LOST_STAT; i < OPERF_MAX_STATS; i++) {
		if (stats_dir_valid && operf_stats[i])
			_write_stats_file(stats_dir + "/" + stats_filenames[i], operf_stats[i]);
		total_lost_samples += operf_stats[i];
	}
	// Write total_samples into stats file if we see any indication of lost samples
	if (total_lost_samples)
		_write_stats_file(stats_dir + "/" + stats_filenames[OPERF_SAMPLES], operf_stats[OPERF_SAMPLES]);

	if (total_lost_samples > (int)(OPERF_WARN_LOST_SAMPLES_THRESHOLD
				       * operf_stats[OPERF_SAMPLES])) {
		fprintf(stderr, "\nWARNING: Lost samples detected! See %s for details.\n", operf_log.c_str());
		fprintf(stderr, "Lowering the sampling rate may reduce or eliminate lost samples.\n");
		fprintf(stderr, "See the '--events' option description in the operf man page for help.\n");
	}
	fflush(fp);
	fclose(fp);
};

static void write_throttled_event_files(vector< operf_event_t> const & events,
                                        string const & stats_dir)
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
}


static string create_stats_dir(string const & cur_sampledir)
{
	int rc;
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
