/**
 * @file op_lockfile.h
 * PID-based lockfile management
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 */

#ifndef OP_LOCKFILE_H
#define OP_LOCKFILE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>

pid_t op_read_lock_file(char const * file);
int op_write_lock_file(char const * file);

#ifdef __cplusplus
}
#endif

#endif /* OP_LOCKFILE_H */
