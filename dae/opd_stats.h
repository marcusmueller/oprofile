/**
 * @file opd_stats.h
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
	OPD_LOST_PROCESS, /* nr. samples for which process info couldn't be accessed */
	OPD_PROCESS, /* nr. userspace samples */
	OPD_LOST_MAP_PROCESS, /* nr. samples for which map info couldn't be accessed */
	OPD_PROC_QUEUE_ACCESS, /* nr. accesses of proc queue */
	OPD_PROC_QUEUE_DEPTH, /* cumulative depth of proc queue accesses */
	OPD_DUMP_COUNT, /* nr. of times buffer is read */
	OPD_MAP_ARRAY_ACCESS, /* nr. accesses of map array */
	OPD_MAP_ARRAY_DEPTH, /* cumulative depth of map array accesses */
	OPD_SAMPLES, /* nr. distinct samples */
	OPD_SAMPLE_COUNTS, /* nr. total individual samples */
	OPD_NOTIFICATIONS, /* nr. notifications */
	OPD_MAX_STATS /* end of stats */
	};

void opd_print_stats(void);

#endif /* OPD_STATS_H */
