/**
 * @file daemon/opd_stats.h
 * Management of daemon statistics
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 */

#ifndef OPD_STATS_H
#define OPD_STATS_H

extern unsigned long opd_stats[];

enum {  OPD_KERNEL, /* nr. kernel samples */
	OPD_MODULE, /* nr. module samples */
	OPD_LOST_MODULE, /* nr. samples in module for which modules can not be located */
	OPD_NO_MM, /* nr. userspace samples for which no mm/cookie was given */
	OPD_PROCESS, /* nr. userspace samples */
	OPD_DUMP_COUNT, /* nr. of times buffer is read */
	OPD_SAMPLES, /* nr. distinct samples */
	OPD_SAMPLE_COUNTS, /* nr. total individual samples */
	OPD_MAX_STATS /* end of stats */
	};

void opd_print_stats(void);

#endif /* OPD_STATS_H */
