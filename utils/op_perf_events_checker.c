/*
 * op-perf-events-checker.c
 *
 *  Created on: Oct 31, 2011
 *      Author: mpj
 */

#include <sys/types.h>
#include <unistd.h>
#include <asm/unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include "config.h"
#if HAVE_PERF_EVENTS
#include <linux/perf_event.h>
#endif

static void usage(void)
{
	fprintf(stderr, "usage: op-check-perfevents [OPTION]\n");
	fprintf(stderr, "\t-h, --help\t\tPrint this help message\n");
	fprintf(stderr, "\t-v, --verbose\t\tPrint errno value of perf_event_open syscall\n");
}

int main(int argc, char **argv)
{
	int _verbose = 0;
	if (argc > 1) {
		if ((!strcmp(argv[1], "-h")) || (!strcmp(argv[1], "--help"))) {
			usage();
			return 0;
		} else if ((!strcmp(argv[1], "-v")) || (!strcmp(argv[1], "--verbose"))) {
			_verbose = 1;
		} else {
			usage();
			return -1;
		}
	}

#if HAVE_PERF_EVENTS
	/* If perf_events syscall is not implemented, the syscall below will fail
	 * with ENOSYS (38).  If implemented, but the processor type on which this
	 * program is running is not supported by perf_events, the syscall returns
	 * ENOENT (2).
	 */
	struct perf_event_attr attr;
	pid_t pid ;
        memset(&attr, 0, sizeof(attr));
        attr.size = sizeof(attr);
        attr.sample_type = PERF_SAMPLE_IP;

	pid = getpid();
	syscall(__NR_perf_event_open, &attr, pid, 0, -1, 0);
	if (_verbose)
		fprintf(stderr, "perf_event_open syscall returned %s\n", strerror(errno));
	return errno;
#else
	return -1;
#endif
}

