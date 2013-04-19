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
#include <vector>
#include "operf_counter.h"

#ifndef OPERF_STATS_H
#define OPERF_STATS_H

extern unsigned long operf_stats[];

void operf_print_stats(std::string sampledir, char * starttime, bool throttled,
                       std::vector< operf_event_t> const & events);

#endif /* OPERF_STATS_H */
