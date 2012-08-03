/**
 * @file libperf_events/operf_stats.h
 * Management of operf statistics
 *
 * @remark Copyright 2012 OProfile authors
 * @remark Read the file COPYING
 *
 * Created on: June 11, 2012
 * @author Maynard Johnson
 * (C) Copyright IBM Corp. 2012
 */

#include <string>

#ifndef OPERF_STATS_H
#define OPERF_STATS_H

extern unsigned long operf_stats[];

enum {	OPERF_SAMPLES, /**< nr. samples */
	OPERF_KERNEL, /**< nr. kernel samples */
	OPERF_PROCESS, /**< nr. userspace samples */
	OPERF_INVALID_CTX, /**< nr. samples lost due to sample address not in expected range for domain */
	OPERF_LOST_KERNEL,  /**< nr. kernel samples lost */
	OPERF_LOST_SAMPLEFILE, /**< nr samples for which sample file can't be opened */
	OPERF_LOST_NO_MAPPING, /**< nr samples lost due to no mapping */
	OPERF_NO_APP_KERNEL_SAMPLE, /**<nr. user ctx kernel samples dropped due to no app context available */
	OPERF_NO_APP_USER_SAMPLE, /**<nr. user samples dropped due to no app context available */
	OPERF_BT_LOST_NO_MAPPING, /**<nr. backtrace samples dropped due to no mapping */
	OPERF_LOST_INVALID_HYPERV_ADDR, /**<nr. hypervisor samples dropped due to address out-of-range */
	OPERF_RECORD_LOST_SAMPLE, /**<nr. samples lost reported by perf_events kernel */
	OPERF_MAX_STATS /**< end of stats */
};
#define OPERF_INDEX_OF_FIRST_LOST_STAT 3

/* Warn on lost samples if number of lost samples is greater the this fraction
 * of the total samples
*/
#define OPERF_WARN_LOST_SAMPLES_THRESHOLD   0.0001

void operf_print_stats(std::string sampledir, char * starttime, bool throttled);

#endif /* OPERF_STATS_H */
