/**
 * @file op_sample_file.h
 * Sample file format
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon <moz@compsoc.man.ac.uk>
 * @author Philippe Elie <phil_el@wanadoo.fr>
 */

#ifndef OP_SAMPLE_FILE_H
#define OP_SAMPLE_FILE_H

#include "op_types.h"

#include <time.h>

/* this char replaces '/' in sample filenames */
#define OPD_MANGLE_CHAR '}'

/* header of the sample files */
struct opd_header {
	u8  magic[4];
	u32 version;
	u8 is_kernel;
	u32 ctr_event;
	u32 ctr_um;
	/* ctr number, used for sanity checking */
	u32 ctr;
	u32 cpu_type;
	u32 ctr_count;
	double cpu_speed;
	time_t mtime;
	int separate_samples;
	/* binary compatibility reserve */
	u32 reserved1[20];
};

struct opd_fentry {
	u32 count;
};

#endif /* OP_SAMPLE_FILE_H */
