/**
 * @file daemon/opd_util.c
 * Code shared between 2.4 and 2.6 daemons
 *
 * @remark Copyright 2002, 2003 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 */

#include <config.h>
 
#include "opd_util.h"
#include "opd_printf.h"

#include "op_config.h"
#include "op_hw_config.h"
#include "op_libiberty.h"
#include "op_abi.h"
#include "op_string.h"
#include "op_cpu_type.h"

#include <sys/types.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

extern op_cpu cpu_type;

sig_atomic_t signal_alarm;
sig_atomic_t signal_hup;
sig_atomic_t signal_term;
sig_atomic_t signal_usr1;
sig_atomic_t signal_usr2;

struct opd_event opd_events[OP_MAX_COUNTERS];

void opd_open_logfile(void)
{
	if (open(OP_LOG_FILE, O_WRONLY|O_CREAT|O_NOCTTY|O_APPEND, 0755) == -1) {
		perror("oprofiled: couldn't re-open stdout: ");
		exit(EXIT_FAILURE);
	}

	if (dup2(1, 2) == -1) {
		perror("oprofiled: couldn't dup stdout to stderr: ");
		exit(EXIT_FAILURE);
	}
}
 

/**
 * opd_fork - fork and return as child
 *
 * fork() and exit the parent with _exit().
 * Failure is fatal.
 */
static void opd_fork(void)
{
	switch (fork()) {
		case -1:
			perror("oprofiled: fork() failed: ");
			exit(EXIT_FAILURE);
			break;
		case 0:
			break;
		default:
			/* parent */
			_exit(EXIT_SUCCESS);
			break;
	}
}

 
void opd_go_daemon(void)
{
	opd_fork();

	if (chdir(OP_BASE_DIR)) {
		fprintf(stderr,"oprofiled: opd_go_daemon: couldn't chdir to "
			OP_BASE_DIR ": %s", strerror(errno));
		exit(EXIT_FAILURE);
	}

	if (setsid() < 0) {
		perror("oprofiled: opd_go_daemon: couldn't setsid: ");
		exit(EXIT_FAILURE);
	}

	opd_fork();
}


void opd_write_abi(void)
{
#ifdef OPROF_ABI
	char * cbuf;
 
	cbuf = xmalloc(strlen(OP_BASE_DIR) + 5);
	strcpy(cbuf, OP_BASE_DIR);
	strcat(cbuf, "/abi");
	op_write_abi_to_file(cbuf);
	free(cbuf);
#endif
}


/**
 * opd_alarm - sync files and report stats
 */
static void opd_alarm(int val __attribute__((unused)))
{
	signal_alarm = 1;
}
 

/* re-open logfile for logrotate */
static void opd_sighup(int val __attribute__((unused)))
{
	signal_hup = 1;
}


static void opd_sigterm(int val __attribute__((unused)))
{
	signal_term = 1;
}
 

static void opd_sigusr1(int val __attribute__((unused)))
{
	signal_usr1 = 1;
}

 
static void opd_sigusr2(int val __attribute__((unused)))
{
	signal_usr2 = 1;
}


void opd_setup_signals(void)
{
	struct sigaction act;
 
	act.sa_handler = opd_alarm;
	act.sa_flags = 0;
	sigemptyset(&act.sa_mask);

	if (sigaction(SIGALRM, &act, NULL)) {
		perror("oprofiled: install of SIGALRM handler failed: ");
		exit(EXIT_FAILURE);
	}

	act.sa_handler = opd_sighup;
	act.sa_flags = 0;
	sigemptyset(&act.sa_mask);
	sigaddset(&act.sa_mask, SIGALRM);

	if (sigaction(SIGHUP, &act, NULL)) {
		perror("oprofiled: install of SIGHUP handler failed: ");
		exit(EXIT_FAILURE);
	}

	act.sa_handler = opd_sigterm;
	act.sa_flags = 0;
	sigemptyset(&act.sa_mask);
	sigaddset(&act.sa_mask, SIGTERM);

	if (sigaction(SIGTERM, &act, NULL)) {
		perror("oprofiled: install of SIGTERM handler failed: ");
		exit(EXIT_FAILURE);
	}

	act.sa_handler = opd_sigusr1;
	act.sa_flags = 0;
	sigemptyset(&act.sa_mask);
	sigaddset(&act.sa_mask, SIGTERM);

	if (sigaction(SIGUSR1, &act, NULL)) {
		perror("oprofiled: install of SIGUSR1 handler failed: ");
		exit(EXIT_FAILURE);
	}

	act.sa_handler = opd_sigusr2;
	act.sa_flags = 0;
	sigemptyset(&act.sa_mask);
	sigaddset(&act.sa_mask, SIGTERM);

	if (sigaction(SIGUSR2, &act, NULL)) {
		perror("oprofiled: install of SIGUSR1 handler failed: ");
		exit(EXIT_FAILURE);
	}

	/* clean up every 10 minutes */
	alarm(60*10);
}


static void malformed_events(void)
{
	fprintf(stderr, "oprofiled: malformed events passed "
	        "on the command line\n");
	exit(EXIT_FAILURE);
}


static char * copy_token(char ** c, char delim)
{
	char * tmp = *c;
	char * tmp2 = *c;
	char * str;

	if (!**c)
		return NULL;

	while (*tmp2 && *tmp2 != delim)
		++tmp2;

	if (tmp2 == tmp)
		return NULL;

	str = op_xstrndup(tmp, tmp2 - tmp);
	*c = tmp2;
	if (**c)
		++*c;
	return str;
}


static unsigned long copy_ulong(char ** c, char delim)
{
	unsigned long val = 0;
	char * str = copy_token(c, delim);
	if (!str)
		malformed_events();
	val = strtoul(str, NULL, 0);
	free(str);
	return val;
}



/** opd_parse_events - parse the events list */
void opd_parse_events(char const * events)
{
	char * ev = xstrdup(events);
	char * c;
	size_t cur = 0;

	if (cpu_type == CPU_TIMER_INT) {
		struct opd_event * event = &opd_events[0];
		event->name = xstrdup("TIMER");
		event->value = event->counter
			= event->count = event->um = 0;
		event->kernel = 1;
		event->user = 1;
		return;
	}

	if (!ev || !strlen(ev)) {
		fprintf(stderr, "oprofiled: no events passed.\n");
		exit(EXIT_FAILURE);
	}

	verbprintf("Events: %s\n", ev);

	c = ev;

	while (*c) {
		struct opd_event * event = &opd_events[cur];

		if (!(event->name = copy_token(&c, ':')))
			malformed_events();
		event->value = copy_ulong(&c, ':');
		event->counter = copy_ulong(&c, ':');
		event->count = copy_ulong(&c, ':');
		event->um = copy_ulong(&c, ':');
		event->kernel = copy_ulong(&c, ':');
		event->user = copy_ulong(&c, ',');
		++cur;
	}

	free(ev);

	/* FIXME: validation ? */
}
