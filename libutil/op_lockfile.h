/**
 * @file op_lockfile.h
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 * 
 * @author John Levon <moz@compsoc.man.ac.uk>
 * @author Philippe Elie <phil_el@wanadoo.fr>
 */

#ifndef OP_LOCKFILE_H
#define OP_LOCKFILE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>
 
pid_t op_read_lock_file(const char * file);
int op_write_lock_file(const char * file);

#ifdef __cplusplus
}
#endif

#endif /* OP_LOCKFILE_H */
