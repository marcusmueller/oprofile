/**
 * @file op_lockfile.c
 * PID-based lockfile management
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 * 
 * @author John Levon <moz@compsoc.man.ac.uk>
 * @author Philippe Elie <phil_el@wanadoo.fr>
 */

#include "op_lockfile.h"
#include "op_file.h"

#include <errno.h>
 
#include <sys/unistd.h>
#include <sys/types.h>
#include <signal.h>
 
/**
 * op_read_lock_file - read a lock file
 *
 * Return the pid written in the given lock file,
 * or 0 if it doesn't exist.
 */
pid_t op_read_lock_file(char const * file)
{
	FILE * fp;
	pid_t value;
 
	fp = fopen(file, "r");
	if (fp == NULL)
		return 0;
 
	if (fscanf(fp, "%d", &value) != 1) {
	        fclose(fp);
		return 0;
        }
 
	fclose(fp);
 
        return value;
}
 
 
/**  
 * op_write_lock_file - write a lock file
 * \return errno on failure, or 0 on success
 *
 * Write the pid into the given lock file. Stale
 * lock files are detected and reset.
 */ 
int op_write_lock_file(char const * file)
{
	FILE * fp;

	if (op_get_fsize(file, 0) != 0) {
		pid_t pid = op_read_lock_file(file);
 
		/* FIXME: ESRCH vs. EPERM */
		if (kill(pid, 0)) {
			int err = unlink(file);
			fprintf(stderr, "Removing stale lock file %s\n",
				file);
			if (err)
				return err;
		} else {
			return EEXIST;
		}
	}
 
	fp = fopen(file, "w");
	if (!fp)
		return errno;

	fprintf(fp, "%d", getpid());
	fclose(fp);

	return 0;
}
