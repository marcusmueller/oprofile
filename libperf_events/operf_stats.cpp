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

void operf_stats_recorder::mv_multiplexed_data_dir(std::string const & sessiondir,
						   std::string const & statsdir)
{
	string old_dir = sessiondir + "/multiplexed/";
	string new_dir = statsdir + "/multiplexed";

	/* The multiplexed data directory was created under the session
	 * directory by another process.  Once the process that calls the
	 * function convert_sample_data() to set up the current data directory
	 * is done and the other process has finished writing to the
	 * multiplexed data directory, the multiplexed data directory can
	 * be moved from sessiondir/multiplexed to statsdir/multiplexed.
	 */
	if (rename(old_dir.c_str(), new_dir.c_str()) < 0) {
		if (errno && (errno != ENOENT)) {
			cerr << "Unable to move multiplexed data dir to "
				<< new_dir << endl;
			cerr << strerror(errno) << endl;
		}
	}
}


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


void operf_stats_recorder::check_for_multiplexing(std::vector<operf_counter> const & perfCounters,
						  int num_cpus,
						  int system_wide, int evt)
{
  /* In system-wide-mode, all events are enabled and running on CPUs if
   * there are counters available.  Hence on each CPU the running and enabled
   * time should be equal if the event was not multiplexed.
   *
   * In non system-wide-mode, there is a specified pid or process that is
   * being monitored.  In this case, the running time across all CPUs that
   * the process ran should be equal to the enabled time if the event was not
   * multiplexed.  Note, we need to account for the fact that the enabled time
   * might vary slightly across CPUs.
   */
	struct {
		u64 count;
		u64 time_enabled; // PERF_FORMAT_TOTAL_TIME_ENABLED is set in attr
		u64 time_running; // PERF_FORMAT_TOTAL_TIME_RUNNING is set in attr
		u64 id; // PERF_FORMAT_ID is set in attr
	} read_data;

	u64 cumulative_time_running = 0;
	u64 max_enabled_time = 0, min_enabled_time = 0xFFFFFFFFFFFFFFFFULL;
	int fd, idx_for_event_name;
	bool event_multiplexed = false;

	for (unsigned int i = 0; i < perfCounters.size(); i++) {
		if (perfCounters[i].get_evt_num() != evt)
			continue;
		// Any index can be used, since event name is the same
		idx_for_event_name = i;
		fd = perfCounters[i].get_fd();

		if (read(fd, &read_data, sizeof(read_data)) == -1) {
			return;
		}

		cumulative_time_running += read_data.time_running;

		if (min_enabled_time > read_data.time_enabled)
			min_enabled_time = read_data.time_enabled;

		if (max_enabled_time < read_data.time_enabled)
			max_enabled_time = read_data.time_enabled;

		if (system_wide &&
		    (read_data.time_enabled != read_data.time_running)) {
			event_multiplexed = true;
			break;
		}
	}

	if (!system_wide) {
		/* Check that the cumulative running time and the enabled
		 * time from the last CPU match.  If they don't match, then
		 * the event was multiplexed on one or more of the CPUs.
		 */
		if ((cumulative_time_running < min_enabled_time) ||
		    (cumulative_time_running > max_enabled_time))
			event_multiplexed = true;
	}

	if (event_multiplexed) {
		/* Create a file with the name of the event in the multiplexed
		 * directory to indicate the event was multiplexed.  Currently
		 * no information is recorded in the file.
		 */
		string event_name;
		string multiplexed_dir, samples_dir;
		string outputfile;
		ofstream outfile;
		int rc;

		/* Have a multiplexed event, create a directory to store the
		 * multiplexed file names in.  The directory will temporarily
		 * be right under the session dir.  The issue is there are two
		 * processes running.  The other process handles moving the
		 * sample data files, outputting the profile data and writing
		 * the throttled data.  This process is independent of it.
		 * This process has access to the session_dir as it was setup
		 * before the fork.  However, we do not have access to the
		 * path where the data will be stored nor do we know when the
		 * previous data will get copied to the previous data
		 * directory. So, we write the multiplexed event data under the
		 * session dir and let the other process move it to the current
		 * sample directory later.
		 */
		multiplexed_dir = operf_options::session_dir + "/multiplexed";

		rc = mkdir(multiplexed_dir.c_str(), S_IRWXU | S_IRWXG
			   | S_IROTH | S_IXOTH);
		if (rc && (errno != EEXIST)) {
			cerr << "Error trying to create multiplexed "
			     << "dir: " << multiplexed_dir << "." << endl;
			perror("mkdir failed with");
			return;
		}

		event_name = perfCounters[idx_for_event_name].get_event_name();
		outputfile = multiplexed_dir + "/" + event_name;

		outfile.open(outputfile.c_str());

		if (!outfile.is_open()) {
			cerr << "Internal error: Could not create " << outputfile
			     << strerror(errno) << endl;
		} else {
			outfile.close();
		}
	}
};
