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

struct opd_map;
struct opd_image;
struct op_note;
struct op_sample;

struct opd_proc {
	/* maps are stored in such order than maps[0] is the mapping
	 * for the primary image (so on maps are not ordered by vma) */
	struct opd_map * maps;
	unsigned int nr_maps;
	unsigned int max_nr_maps;
	unsigned int last_map;
	u32 pid;
	int accessed;
	int dead;
	struct opd_proc * prev;
	struct opd_proc * next;
};

void opd_put_sample(struct op_sample const * sample);
void opd_put_image_sample(struct opd_image * image, unsigned long offset, u32 counter);
void opd_handle_fork(struct op_note const * note);
void opd_handle_exit(struct op_note const * note);
void opd_handle_exec(u32 pid);
struct opd_proc * opd_get_proc(u32 pid);
struct opd_proc * opd_add_proc(u32 pid);
char const * opd_app_name(struct opd_proc const * proc);
int opd_get_nr_procs(void);
void opd_age_procs(void);
void opd_proc_cleanup(void);
void opd_clear_kernel_mapping(void);

#endif /* OPD_PROC_H */
