/**
 * @file dae/opd_proc.c
 * Management of processes
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 */

#include "opd_proc.h"
#include "opd_image.h"
#include "opd_mapping.h"
#include "opd_sample_files.h"
#include "opd_kernel.h"
#include "opd_stats.h"
#include "opd_printf.h"

#include "op_interface.h"
#include "op_cpu_type.h"
#include "op_libiberty.h"

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* size of process hash table */
#define OPD_MAX_PROC_HASH 1024

extern int separate_lib;
extern int separate_kernel;
extern int separate_thread;
extern int no_vmlinux;
extern int cpu_number;

/* hash of process lists */
static struct list_head opd_procs[OPD_MAX_PROC_HASH];

/* statistics purpose */
static int nr_procs;


/**
 * opd_init_proc() init proc hash table
 */
void opd_init_proc(void)
{
	int i;

	for (i = 0; i < OPD_MAX_PROC_HASH; i++)
		list_init(&opd_procs[i]);
}


/**
 * opd_get_nr_procs - return number of processes tracked
 */
int opd_get_nr_procs(void)
{
	return nr_procs;
}


/**
 * proc_hash - hash pid value
 * @param tid  pid value to hash
 *
 */
inline static uint proc_hash(pid_t tid)
{
	/* FIXME: hash tgid too! */
	return ((tid>>4) ^ (tid)) % OPD_MAX_PROC_HASH;
}


/**
 * opd_new_proc - create a new process structure
 * @param tid  tid for this process
 * @param tgid  tgid for this process
 *
 * Allocate and initialise a process structure and insert
 * it into the procs hash table.
 */
struct opd_proc * opd_new_proc(pid_t tid, pid_t tgid)
{
	struct opd_proc * proc;

	nr_procs++;
	proc = xmalloc(sizeof(struct opd_proc));
	list_init(&proc->maps);
	proc->name = NULL;
	proc->tid = tid;
	proc->tgid = tgid;
	proc->dead = 0;
	proc->accessed = 0;
	list_add(&proc->next, &opd_procs[proc_hash(tid)]);
	return proc;
}


/**
 * opd_delete_proc - delete a process
 * @param proc  process to delete
 *
 * Remove the process proc from the process list and free
 * the associated structures.
 */
static void opd_delete_proc(struct opd_proc * proc)
{
	--nr_procs;
	list_del(&proc->next);
	opd_kill_maps(proc);
	if (proc->name)
		free((char *)proc->name);
	free(proc);
}


/**
 * opd_age_proc - age a struct opd_proc
 * @param  proc proc to age
 *
 * age dead proc in such way if a proc doesn't receive any samples
 * between two age_proc the opd_proc struct is deleted
 */
void opd_age_proc(struct opd_proc * proc)
{
	// delay death whilst its still being accessed
	if (proc->dead) {
		proc->dead += proc->accessed;
		proc->accessed = 0;
		if (--proc->dead == 0)
			opd_delete_proc(proc);
	}
}

/**
 * @param proc_cb callback to apply onto each existing proc struct
 *
 * the callback receive a struct opd_proc * (not a const struct) and is
 * allowed to freeze the proc struct itself.
 */
void opd_for_each_proc(opd_proc_cb proc_cb)
{
	struct list_head * pos;
	struct list_head * pos2;
	int i;

	for (i = 0; i < OPD_MAX_PROC_HASH; ++i) {
		list_for_each_safe(pos, pos2, &opd_procs[i]) {
			struct opd_proc * proc =
				list_entry(pos, struct opd_proc, next);
			proc_cb(proc);
		}
	}
}


/**
 * opd_get_proc - get process from process list
 * @param pid  pid to search for
 *
 * A process with pid pid is searched on the process list,
 * maintaining LRU order. If it is not found, %NULL is returned,
 * otherwise the process structure is returned.
 */
struct opd_proc * opd_get_proc(pid_t tid, pid_t tgid)
{
	struct opd_proc * proc;
	uint hash = proc_hash(tid);
	struct list_head * pos, *pos2;

	opd_stats[OPD_PROC_QUEUE_ACCESS]++;
	list_for_each_safe(pos, pos2, &opd_procs[hash]) {
		opd_stats[OPD_PROC_QUEUE_DEPTH]++;
		proc = list_entry(pos, struct opd_proc, next);
		if (tid == proc->tid && tgid == proc->tgid) {
			/* LRU to head */
			list_del(&proc->next);
			list_add(&proc->next, &opd_procs[hash]);
			return proc;
		}
	}

	return NULL;
}


/**
 * verb_show_sample - print the sample out to the log
 * @param offset  the offset value
 * @param map  map to print
 */
inline static void
verb_show_sample(unsigned long offset, struct opd_map * map)
{
	verbprintf("DO_PUT_SAMPLE : calc offset 0x%.8lx, map start 0x%.8lx,"
		" end 0x%.8lx, offset 0x%.8lx, name \"%s\"\n",
		   offset, map->start, map->end, map->offset, 
		   map->image->name);
}


/**
 * opd_put_image_sample - write sample to file
 * @param image  image for sample
 * @param offset  (file) offset to write to
 * @param counter  counter number
 *
 * Add to the count stored at position offset in the
 * image file. Overflow pins the count at the maximum
 * value.
 */
void opd_put_image_sample(struct opd_image * image, unsigned long offset,
                          u32 counter)
{
	struct opd_sfile * sfile;
	int err;

	sfile = image->sfiles[counter][cpu_number];

	if (!sfile || !sfile->sample_file.base_memory) {
		if (opd_open_sample_file(image, counter, cpu_number)) {
			/* opd_open_sample_file output an error message */
			return;
		}
		sfile = image->sfiles[counter][cpu_number];
	}

	err = odb_insert(&sfile->sample_file, offset, 1);
	if (err) {
		fprintf(stderr, "%s\n", strerror(err));
		abort();
	}

	opd_sfile_lru(sfile);
}


/**
 * opd_lookup_maps - lookup a proc mappings for a sample
 * @param proc proc to lookup
 * @param sample sample to lookup
 *
 * iterate through the proc maps searching the mapping which owns sample
 * if sucessful sample count will be updated and we return non-zero
 */
static int opd_lookup_maps(struct opd_proc * proc,
			struct op_sample const * sample)
{
	struct list_head * pos;

	proc->accessed = 1;

	opd_stats[OPD_MAP_ARRAY_ACCESS]++;
	list_for_each(pos, &proc->maps) {
		struct opd_map * map = list_entry(pos, struct opd_map, next);
		if (opd_is_in_map(map, sample->eip)) {
			unsigned long offset = opd_map_offset(map, sample->eip);
			if (map->image != NULL) {
				verb_show_sample(offset, map);
				opd_put_image_sample(map->image, offset, sample->counter);
			}
			opd_stats[OPD_PROCESS]++;
			return 1;
		}
		opd_stats[OPD_MAP_ARRAY_DEPTH]++;
	}

	return 0;
}


/**
 * opd_put_sample - process a sample
 * @param sample  sample to process
 *
 * Write out the sample to the appropriate sample file. This
 * routine handles kernel and module samples as well as ordinary ones.
 */
void opd_put_sample(struct op_sample const * sample)
{
	extern int kernel_only;

	struct opd_proc * proc;
	int in_kernel_eip = opd_eip_is_kernel(sample->eip);

	opd_stats[OPD_SAMPLES]++;

	verbprintf("DO_PUT_SAMPLE: c%d, EIP 0x%.8lx, tgid %.6d pid %.6d\n",
		sample->counter, sample->eip, sample->tgid, sample->pid);

	if (!separate_kernel && in_kernel_eip) {
		opd_handle_kernel_sample(sample->eip, sample->counter);
		return;
	}

	if (kernel_only && !in_kernel_eip)
		return;

	if (!(proc = opd_get_proc(sample->pid, sample->tgid))) {
		if (in_kernel_eip || no_vmlinux) {
			/* idle task get a 0 pid and is hidden we can never get
			 * a proc so on we fall back to put sample in vmlinux
			 * or module samples files. Here we will catch also
			 * sample for newly created kernel thread, currently 
			 * we can handle properly only kenel thread created
			 * at daemon startup time */
			opd_handle_kernel_sample(sample->eip, sample->counter);
		} else {
			verbprintf("No proc info for tgid %.6d pid %.6d.\n",
                                   sample->tgid, sample->pid);
			opd_stats[OPD_LOST_PROCESS]++;
		}
		return;
	}

	if (opd_lookup_maps(proc, sample))
		return;

	if (in_kernel_eip) {
		/* assert: separate_kernel || no_vmlinux == 0 */
		opd_add_kernel_map(proc, sample->eip);
		if (opd_lookup_maps(proc, sample))
			return;
	}

	if (no_vmlinux) {
		/* in_kernel_eip can't be true when no_vmlinux != 0, we handle
		 * now all unknown samples and they go blindly to no-vmlinux */
		opd_handle_kernel_sample(sample->eip, sample->counter);
		return;
	}

	/* couldn't locate it */
	verbprintf("Couldn't find map for pid %.6d, EIP 0x%.8lx.\n",
		   sample->pid, sample->eip);
	opd_stats[OPD_LOST_MAP_PROCESS]++;
}


/**
 * opd_handle_fork - deal with fork notification
 * @param note  note to handle
 *
 * Deal with a fork() notification by creating a new process
 * structure, and copying mapping information from the old process.
 *
 * sample->pid contains the process id of the old process.
 * sample->eip contains the process id of the new process.
 */
void opd_handle_fork(struct op_note const * note)
{
	struct opd_proc * old;
	struct opd_proc * proc;
	struct list_head * pos;

	verbprintf("DO_FORK: from %d, %d to %ld, %ld\n", note->pid, note->tgid,
	           note->addr, note->len);

	old = opd_get_proc(note->pid, note->tgid);

	/* we can quite easily get a fork() after the execve() because the
	 * notifications are racy. In particular, the fork notification is
	 * done on parent return (so we know the pid), but this will often be
	 * after the execve is done by the child.
	 *
	 * So we only create a new setup if it doesn't exist already, allowing
	 * both the clone() and the execve() cases to work.
	 */
	if (opd_get_proc(note->addr, note->len))
		return;

	/* eip/len is actually tid/tgid of new process */
	proc = opd_new_proc(note->addr, note->len);

	if (!old)
		return;

	/* copy the maps */
	list_for_each(pos, &old->maps) {
		struct opd_map * map = list_entry(pos, struct opd_map, next);
		if (!separate_thread) {
			opd_add_mapping(proc, map->image, map->start,
			                map->offset, map->end);
		} else {
			/* when separating thread we can't create blindly a new
			 * image e.g. pid re-use, multiple mapping with the
			 * same mapping name etc. */
			struct opd_image * image = 
			   opd_get_image(map->image->name, old->name,
			                 map->image->kernel, note->addr,
			                 note->len);
			opd_add_mapping(proc, image, map->start, map->offset,
			                map->end);
		}
	}
}


/**
 * opd_handle_exec - deal with notification of execve()
 * @param pid  pid of execve()d process
 *
 * Drop all mapping information for the process.
 */
void opd_handle_exec(pid_t tid, pid_t tgid)
{
	struct opd_proc * proc;

	verbprintf("DO_EXEC: pid %u %u\n", tid, tgid);

	/* There is a race for samples received between fork/exec sequence.
	 * These samples belong to the old mapping but we can not say if
	 * samples has been received before the exec or after. This explain
	 * the message "Couldn't find map for ..." in verbose mode.
	 *
	 * Unhopefully it is difficult to get an estimation of these misplaced
	 * samples, the error message can count only out of mapping samples but
	 * not samples between the race and inside the mapping of the exec'ed
	 * process :/.
	 *
	 * Trying to save old mapping is not correct due the above reason. The
	 * only manner to handle this is to flush the module samples hash table
	 * after each fork which is unacceptable for performance reasons */
	proc = opd_get_proc(tid, tgid);
	if (proc) {
		opd_kill_maps(proc);
		/* proc->name will be set when the next mapping occurs */
		free((char *)proc->name);
		proc->name = NULL;
	} else {
		opd_new_proc(tid, tgid);
	}
}


/**
 * opd_handle_exit - deal with exit notification
 * @param note  note to handle
 *
 * Deal with an exit() notification by setting the flag "dead"
 * on a process. These will be later cleaned up by the %SIGALRM
 * handler.
 *
 * sample->pid contains the process id of the exited process.
 */
void opd_handle_exit(struct op_note const * note)
{
	struct opd_proc * proc;

	verbprintf("DO_EXIT: process %d\n", note->pid);

	proc = opd_get_proc(note->pid, note->tgid);
	if (proc) {
		proc->dead = 1;
		proc->accessed = 1;
	} else {
		verbprintf("unknown proc %u just exited.\n", note->pid);
	}
}


/**
 * opd_proc_cleanup - clean up on exit
 */
void opd_proc_cleanup(void)
{
	opd_for_each_proc(opd_delete_proc);
}


/**
 * opd_remove_kernel_mapping - remove all kernel mapping for an opd_proc
 * @param proc  proc where mappings must be updated.
 *
 * invalidate (by removing them) all kernel mapping. This function do nothing
 * when separate_kernel == 0 because we don't add mapping for kernel
 * sample in proc struct.
 */
static void opd_remove_kernel_mapping(struct opd_proc * proc)
{
	struct list_head * pos, * pos2;

	list_for_each_safe(pos, pos2, &proc->maps) {
		struct opd_map * map = list_entry(pos, struct opd_map, next);
		if (opd_eip_is_kernel(map->start + map->offset)) {
			list_del(pos);
			opd_delete_image(map->image);
			free(map);
		}
	}
}


/**
 * opd_clear_kernel_mapping - remove all kernel mapping for all opd_proc
 *
 * invalidate (by removing them) all kernel mapping. This function do nothing
 * when separate_kernel == 0 because we don't add mapping for kernel
 * sample in proc struct.
 */
void opd_clear_kernel_mapping(void)
{
	opd_for_each_proc(opd_remove_kernel_mapping);
}
