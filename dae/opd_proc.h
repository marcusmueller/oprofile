/**
 * @file dae/opd_proc.h
 * Management of processes
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 */

#ifndef OPD_PROC_H
#define OPD_PROC_H

#include "op_types.h"
#include "op_list.h"

struct opd_map;
struct opd_image;
struct op_note;
struct op_sample;

struct opd_proc {
	/* maps are always added to the end of head, so search will be done
	 * from the newest map to the oldest which mean we don't care about
	 * munmap. First added map must be the primary image */
	struct list_head maps;
	char const * name;
	u32 pid;
	int accessed;
	int dead;
	struct list_head next;
};

void opd_proc_init(void);

void opd_put_sample(struct op_sample const * sample);
void opd_put_image_sample(struct opd_image * image, unsigned long offset, u32 counter);
void opd_handle_fork(struct op_note const * note);
void opd_handle_exit(struct op_note const * note);
void opd_handle_exec(u32 pid);
struct opd_proc * opd_get_proc(u32 pid);
struct opd_proc * opd_new_proc(u32 pid);
int opd_get_nr_procs(void);
void opd_age_procs(void);
void opd_proc_cleanup(void);
void opd_clear_kernel_mapping(void);

#endif /* OPD_PROC_H */
