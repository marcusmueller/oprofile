/**
 * @file daemon/opd_util.c
 * Code shared between 2.4 and 2.5 daemons
 *
 * @remark Copyright 2002, 2003 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 */

#include <config.h>
 
#include "opd_util.h"
#include "op_config.h"

#include <sys/types.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

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

 
/**
 * opd_go_daemon - become daemon process
 *
 * Become an un-attached daemon in the standard
 * way (fork(),chdir(),setsid(),fork()).
 * Parents perform _exit().
 *
 * Any failure is fatal.
 */
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
