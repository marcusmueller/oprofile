/**
 * @file opd_perfmon.c
 * perfmonctl() handling
 *
 * @remark Copyright 2003 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 */

#ifdef __ia64__

#include "opd_util.h"
#include "opd_perfmon.h"

#include "op_libiberty.h"
#include "op_hw_config.h"

#include <sys/syscall.h>
#include <unistd.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/* many glibc's are not yet up to date */
#ifndef __NR_sched_setaffinity
#define __NR_sched_setaffinity 1231
static int sched_setaffinity(pid_t pid, unsigned int len, unsigned long * mask)
{
	return syscall(__NR_sched_setaffinity, pid, len, mask);
}
#endif


#ifndef __NR_perfmonctl
#define __NR_perfmonctl 1175
#endif

static int perfmonctl(int fd, int cmd, void * arg, int narg)
{
	return syscall(__NR_perfmonctl, fd, cmd, arg, narg);
}


static unsigned char uuid[16] = {
	0x77, 0x7a, 0x6e, 0x61, 0x20, 0x65, 0x73, 0x69,
	0x74, 0x6e, 0x72, 0x20, 0x61, 0x65, 0x0a, 0x6c
};


static size_t nr_cpus;

struct child {
	pid_t pid;
	int ctx_fd;
	sig_atomic_t sigusr1;
	sig_atomic_t sigusr2;
	sig_atomic_t sigterm;
};

static struct child * children;

static void perfmon_start_child(int ctx_fd)
{
	if (perfmonctl(ctx_fd, PFM_START, 0, 0) == -1) {
		perror("Couldn't start perfmon: ");
		exit(EXIT_FAILURE);
	}
}


static void perfmon_stop_child(int ctx_fd)
{
	if (perfmonctl(ctx_fd, PFM_STOP, 0, 0) == -1) {
		perror("Couldn't stop perfmon: ");
		exit(EXIT_FAILURE);
	}
}


static void child_sigusr1(int val __attribute__((unused)))
{
	size_t i;

	for (i = 0; i < nr_cpus; ++i) {
		if (children[i].pid == getpid()) {
			children[i].sigusr1 = 1;
			return;
		}
	}
}


static void child_sigusr2(int val __attribute__((unused)))
{
	size_t i;

	for (i = 0; i < nr_cpus; ++i) {
		if (children[i].pid == getpid()) {
			children[i].sigusr2 = 1;
			return;
		}
	}
}


static void child_sigterm(int val __attribute__((unused)))
{
	printf("Child received SIGTERM, killing parent.\n");
	kill(getppid(), SIGTERM);
}


static void run_child(size_t cpu)
{
	struct child * self = &children[cpu];
	unsigned long affinity_mask = 1UL << cpu;
	struct sigaction act;
	pfarg_reg_t pc[OP_MAX_COUNTERS];
	pfarg_reg_t pd[OP_MAX_COUNTERS];
	pfarg_context_t ctx;
	pfarg_load_t load_args;
	size_t i;
	int err;

	self->pid = getpid();
	self->sigusr1 = 0;
	self->sigusr2 = 0;
	self->sigterm = 0;

	/* FIXME: we can still get a signal before this, it's racy. */

	act.sa_handler = child_sigusr1;
	act.sa_flags = 0;
	sigemptyset(&act.sa_mask);

	if (sigaction(SIGUSR1, &act, NULL)) {
		perror("oprofiled: install of SIGUSR1 handler failed: ");
		exit(EXIT_FAILURE);
	}

	act.sa_handler = child_sigusr2;
	act.sa_flags = 0;
	sigemptyset(&act.sa_mask);

	if (sigaction(SIGUSR2, &act, NULL)) {
		perror("oprofiled: install of SIGUSR2 handler failed: ");
		exit(EXIT_FAILURE);
	}

	act.sa_handler = child_sigterm;
	act.sa_flags = 0;
	sigemptyset(&act.sa_mask);

	if (sigaction(SIGTERM, &act, NULL)) {
		perror("oprofiled: install of SIGTERM handler failed: ");
		exit(EXIT_FAILURE);
	}

	err = sched_setaffinity(self->pid, sizeof(unsigned long),
	                        &affinity_mask);

	if (err == -1) {
		fprintf(stderr, "Failed to set affinity\n");
		exit(EXIT_FAILURE);
	}

	memset(&ctx, 0, sizeof(pfarg_context_t));
	memcpy(&ctx.ctx_smpl_buf_id, &uuid, 16);
	ctx.ctx_flags = PFM_FL_SYSTEM_WIDE;

	err = perfmonctl(0, PFM_CREATE_CONTEXT, &ctx, 1);
	if (err == -1) {
		fprintf(stderr, "CREATE_CONTEXT failed: %s\n",
		        strerror(errno));
		exit(EXIT_FAILURE);
	}

	self->ctx_fd = ctx.ctx_fd;

	memset(pc, 0, sizeof(pc));
	memset(pd, 0, sizeof(pd));

#define PMC_GEN_INTERRUPT (1UL << 5)
#define PMC_PRIV_MONITOR (1UL << 6)
/* McKinley requires pmc4 to have bit 23 set (enable PMU).
 * It is supposedly ignored in other pmc registers.
 */
#define PMC_MANDATORY (1UL << 23)
#define PMC_USER (1UL << 3)
#define PMC_KERNEL (1UL << 0)
	for (i = 0; opd_events[i].name; ++i) {
		struct opd_event * event = &opd_events[i];
		pc[i].reg_num = event->counter + 4;
		pc[i].reg_value = PMC_GEN_INTERRUPT;
		pc[i].reg_value |= PMC_PRIV_MONITOR;
		pc[i].reg_value |= PMC_MANDATORY;
		(event->user) ? (pc[i].reg_value |= PMC_USER)
		              : (pc[i].reg_value &= ~PMC_USER);
		(event->kernel) ? (pc[i].reg_value |= PMC_KERNEL)
		                : (pc[i].reg_value &= ~PMC_KERNEL);
		pc[i].reg_value &= ~(0xff << 8);
		pc[i].reg_value |= ((event->value & 0xff) << 8);
		pc[i].reg_value &= ~(0xf << 16);
		pc[i].reg_value |= ((event->um & 0xf) << 16);
		pc[i].reg_smpl_eventid = event->counter;
	}

	for (i = 0; opd_events[i].name; ++i) {
		struct opd_event * event = &opd_events[i];
		pd[i].reg_value = ~0UL - event->count + 1;
		pd[i].reg_short_reset = ~0UL - event->count + 1;
		pd[i].reg_num = event->counter + 4;
	}

	err = perfmonctl(self->ctx_fd, PFM_WRITE_PMCS, pc, i);
	if (err == -1) {
		perror("Couldn't write PMCs: ");
		exit(EXIT_FAILURE);
	}

	err = perfmonctl(self->ctx_fd, PFM_WRITE_PMDS, pd, i);
	if (err == -1) {
		perror("Couldn't write PMDs: ");
		exit(EXIT_FAILURE);
	}

	load_args.load_pid = self->pid;

	err = perfmonctl(self->ctx_fd, PFM_LOAD_CONTEXT, &load_args, 1);
	if (err == -1) {
		perror("Couldn't load context: ");
		exit(EXIT_FAILURE);
	}

	printf("Perfmon child up on CPU%d\n", (int)cpu);

	for (;;) {
		pause();

		if (self->sigusr1) {
			perfmon_start_child(self->ctx_fd);
			self->sigusr1 = 0;
		}

		if (self->sigusr2) {
			perfmon_stop_child(self->ctx_fd);
			self->sigusr2 = 0;
		}
	}
}


void perfmon_init(void)
{
	size_t i;
	long nr;

	nr = sysconf(_SC_NPROCESSORS_ONLN);
	if (nr == -1) {
		fprintf(stderr, "Couldn't determine number of CPUs.\n");
		exit(EXIT_FAILURE);
	}

	children = xmalloc(sizeof(struct child) * nr_cpus);

	nr_cpus = nr;

	for (i = 0; i < nr_cpus; ++i) {
		int ret = fork();
		if (ret == -1) {
			fprintf(stderr, "Couldn't fork perfmon child.\n");
			exit(EXIT_FAILURE);
		} else if (ret == 0) {
			printf("Running perfmon child on CPU%d.\n", (int)i);
			run_child(i);
		} else {
			children[i].pid = ret;
		}
	}
}


void perfmon_exit(void)
{
	size_t i;

	for (i = 0; i < nr_cpus; ++i)
		kill(children[i].pid, SIGKILL);
}


void perfmon_start(void)
{
	size_t i;

	for (i = 0; i < nr_cpus; ++i)
		kill(children[i].pid, SIGUSR1);
}


void perfmon_stop(void)
{
	size_t i;

	for (i = 0; i < nr_cpus; ++i)
		kill(children[i].pid, SIGUSR2);
}

#endif /* __ia64__ */
