/**
 * @file daemon/opd_trans.h
 * Processing the sample buffer
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 */

#ifndef OPD_TRANS_H
#define OPD_TRANS_H

#include "opd_cookie.h"
#include "op_types.h"

struct sfile;

/**
 * Transient values used for parsing the event buffer.
 * Note that these are reset for each buffer read, but
 * that should be ok as in the kernel, cpu_buffer_reset()
 * ensures that a correct context starts off the buffer.
 */
struct transient {
	char const * buffer;
	size_t remaining;
	struct sfile * current;
	cookie_t cookie;
	cookie_t app_cookie;
	vma_t pc;
	int in_kernel;
	unsigned long cpu;
	pid_t tid;
	pid_t tgid;
};

void opd_process_samples(char const * buffer, size_t count);

#endif /* OPD_TRANS_H */
