/**
 * @file op_session.c
 * Manage named profiling sessions
 *
 * @remark Copyright 2002
 * @remark Read the file COPYING
 *
 * @author John Levon
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "version.h"
#include "op_popt.h"
#include "op_libiberty.h"
#include "op_lockfile.h"
#include "op_file.h"
#include "op_config.h"

char const * sessionname;
int showvers;

static struct poptOption options[] = {
	{ "session", 's', POPT_ARG_STRING, &sessionname, 0, "save current session under this name", "session-name", },
	{ "version", 'v', POPT_ARG_NONE, &showvers, 0, "show version", NULL, },
	POPT_AUTOHELP
	{ NULL, 0, 0, NULL, 0, NULL, NULL, },
};


/**
 * op_options - parse command line options
 * @param argc  argc
 * @param argv  argv array
 *
 * Parse all command line arguments.
 */
static void op_options(int argc, char const *argv[])
{
	poptContext optcon;
	char const * file;

	optcon = op_poptGetContext(NULL, argc, argv, options, 0);

	if (showvers) {
		show_version(argv[0]);
	}

	file = poptGetArg(optcon);
	if (file) {
		sessionname = file;
	}

	if (!sessionname) {
		fprintf(stderr, "op_session: no session name specified !\n");
		poptPrintHelp(optcon, stderr, 0);
		exit(EXIT_FAILURE);
	}

	poptFreeContext(optcon);
}


/**
 * op_move_files - move all the sample files
 * @param sname name of session directory
 */
static void op_move_files(char const * sname)
{
	char * dir_name;
	DIR * dir;
	struct dirent * dirent;
	int is_dir_empty = 1;

	dir_name = xmalloc(strlen(OP_SAMPLES_DIR) + strlen(sname) + 1);
	strcpy(dir_name, OP_SAMPLES_DIR);
	strcat(dir_name, sname);

	if (mkdir(dir_name, 0755)) {
		if (errno == EEXIST) {
			fprintf(stderr, "session \"%s\" already exists\n", dir_name);
		} else {
			fprintf(stderr, "unable to create directory \"%s\"\n", dir_name);
		}
		exit(EXIT_FAILURE);
	}

	if (!(dir = opendir(OP_SAMPLES_DIR))) {
		fprintf(stderr, "unable to open directory " OP_SAMPLES_DIR "\n");
		exit(EXIT_FAILURE);
	}

	while ((dirent = readdir(dir)) != 0) {
 		int ret;
 		ret = op_move_regular_file(dir_name, OP_SAMPLES_DIR, dirent->d_name);
 		if (ret < 0) {
			fprintf(stderr, "unable to backup %s/%s to directory %s\n",
			       OP_SAMPLES_DIR, dirent->d_name, dir_name);
			exit(EXIT_FAILURE);
		} else if (ret == 0) {
			is_dir_empty = 0;
		}
	}

	closedir(dir);

 	if (!is_dir_empty) {
 		op_move_regular_file(dir_name, OP_BASE_DIR, OP_LOG_FILE);
 	} else {
 		rmdir(dir_name);

 		fprintf(stderr, "no samples files to save, session %s not created\n",
 			sname);
 		exit(EXIT_FAILURE);
 	}

	free(dir_name);
}


/**
 * op_signal_daemon - signal daemon to re-open if it exists
 */
static void op_signal_daemon(void)
{
	pid_t pid = op_read_lock_file(OP_LOCK_FILE);

	if (pid)
		kill(pid, SIGHUP);
}


int main(int argc, char const *argv[])
{
	pid_t pid;

	op_options(argc, argv);

	fprintf(stderr, "NOTE: op_session is deprecated. Please use "
		"opcontrol --save=%s\n", sessionname);

	/* not ideal, but OK for now. The sleep hopefully
	 * means the daemon starts reading before the signal
	 * is delivered, so it will finish reading, *then*
	 * handle the SIGHUP. Hack ! FIXME
 	 */
 	pid = op_read_lock_file(OP_LOCK_FILE);
 	if (pid) {
 		system("op_dump");
 		sleep(2);
 	}

	op_move_files(sessionname);
	op_signal_daemon();
	return 0;
}
