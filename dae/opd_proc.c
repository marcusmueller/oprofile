/**
 * @file opd_proc.c
 * Management of processes
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon <moz@compsoc.man.ac.uk>
 * @author Philippe Elie <phil_el@wanadoo.fr>
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

/* here to avoid warning */
extern op_cpu cpu_type;

/* hash of process lists */
static struct opd_proc * opd_procs[OPD_MAX_PROC_HASH];

static void opd_delete_proc(struct opd_proc * proc);

/**
 * opd_get_nr_procs - return number of processes tracked
 */
int opd_get_nr_procs(void)
{
	struct opd_proc * proc;
	int i,j = 0;

	for (i=0; i < OPD_MAX_PROC_HASH; i++) {
		proc = opd_procs[i];

		while (proc) {
			++j;
			proc = proc->next;
		}
	}
	return j;
}


/**
 * opd_age_procs - age and delete processes
 *
 * Age processes, and delete any we guess are dead
 */
void opd_age_procs(void)
{
	uint i;
	struct opd_proc * proc;
	struct opd_proc * next;

	for (i=0; i < OPD_MAX_PROC_HASH; i++) {
		proc = opd_procs[i];

		while (proc) {
			next = proc->next;
			// delay death whilst its still being accessed
			if (proc->dead) {
				proc->dead += proc->accessed;
				proc->accessed = 0;
				if (--proc->dead == 0)
					opd_delete_proc(proc);
			}
			proc=next;
		}
	}
}


/**
 * opd_app_name - get the application name or %NULL if irrelevant
 * @param proc  the process to examine
 *
 * Returns the app_name for the given @proc or %NULL if
 * it does not exist any mapping for this proc (which is
 * true for the first mapping at exec time and also true
 * for process parsed through /proc/pid at startup because
 * we reorder maps)
 */
char const * opd_app_name(struct opd_proc const * proc)
{
	char const * app_name = NULL;
	if (proc->nr_maps)
		app_name = proc->maps[0].image->name;

	return app_name;
}

/**
 * opd_new_proc - create a new process structure
 * @param prev  previous list entry
 * @param next  next list entry
 *
 * Allocate and initialise a process structure and insert
 * it into the the list point specified by @prev and @next.
 */
static struct opd_proc * opd_new_proc(struct opd_proc * prev, struct opd_proc * next)
{
	struct opd_proc * proc;

	proc = xmalloc(sizeof(struct opd_proc));
	proc->maps = NULL;
	proc->pid = 0;
	proc->nr_maps = 0;
	proc->max_nr_maps = 0;
	proc->last_map = 0;
	proc->dead = 0;
	proc->accessed = 0;
	proc->prev = prev;
	proc->next = next;
	return proc;
}

/**
 * proc_hash - hash pid value
 * @param pid  pid value to hash
 *
 */
inline static uint proc_hash(u16 pid)
{
	return ((pid>>4) ^ (pid)) % OPD_MAX_PROC_HASH;
}

/**
 * opd_delete_proc - delete a process
 * @param proc  process to delete
 *
 * Remove the process @proc from the process list and free
 * the associated structures.
 */
static void opd_delete_proc(struct opd_proc * proc)
{
	if (!proc->prev)
		opd_procs[proc_hash(proc->pid)] = proc->next;
	else
		proc->prev->next = proc->next;

	if (proc->next)
		proc->next->prev = proc->prev;

	if (proc->maps) free(proc->maps);
	free(proc);
}


/**
 * opd_add_proc - add a process
 * @param pid  process id
 *
 * Create a new process structure and add it
 * to the head of the process list. The process structure
 * is filled in as appropriate.
 *
 */
struct opd_proc * opd_add_proc(u16 pid)
{
	struct opd_proc * proc;
	uint hash = proc_hash(pid);

	proc=opd_new_proc(NULL, opd_procs[hash]);
	if (opd_procs[hash])
		opd_procs[hash]->prev = proc;

	opd_procs[hash] = proc;

	opd_init_maps(proc);
	proc->pid = pid;

	return proc;
}


/**
 * opd_do_proc_lru - rework process list
 * @param head  head of process list
 * @param proc  process to move
 *
 * Perform LRU on the process list by moving it to
 * the head of the process list.
 */
inline static void opd_do_proc_lru(struct opd_proc ** head, struct opd_proc * proc)
{
	if (proc->prev) {
		proc->prev->next = proc->next;
		if (proc->next)
			proc->next->prev = proc->prev;
		(*head)->prev = proc;
		proc->prev = NULL;
		proc->next = *head;
		(*head) = proc;
	}
}

/**
 * opd_get_proc - get process from process list
 * @param pid  pid to search for
 *
 * A process with pid @pid is searched on the process list,
 * maintaining LRU order. If it is not found, %NULL is returned,
 * otherwise the process structure is returned.
 */
struct opd_proc * opd_get_proc(u16 pid)
{
	struct opd_proc * proc;

	proc = opd_procs[proc_hash(pid)];

	opd_stats[OPD_PROC_QUEUE_ACCESS]++;
	while (proc) {
		if (pid == proc->pid) {
			opd_do_proc_lru(&opd_procs[proc_hash(pid)],proc);
			return proc;
		}
		opd_stats[OPD_PROC_QUEUE_DEPTH]++;
		proc = proc->next;
	}

	return NULL;
}


/**
 * verb_show_sample - print the sample out to the log
 * @param offset  the offset value
 * @param map  map to print
 * @param last_map  previous map used
 */
inline static void verb_show_sample(u32 offset, struct opd_map * map, char const * last_map)
{
	verbprintf("DO_PUT_SAMPLE %s: calc offset 0x%.8x, map start 0x%.8x,"
		" end 0x%.8x, offset 0x%.8x, name \"%s\"\n",
		last_map, offset, map->start, map->end, map->offset, map->image->name);
}


/**
 * opd_get_count - retrieve counter value
 * @param count  raw counter value
 *
 * Returns the counter value.
 */
inline static u16 opd_get_count(const u16 count)
{
	return (count & OP_COUNT_MASK);
}


/**
 * opd_get_counter - retrieve counter type
 * @param count  raw counter value
 *
 * Returns the counter number (0-N)
 */
inline static u16 opd_get_counter(const u16 count)
{
	return OP_COUNTER(count);
}


/**
 * opd_put_image_sample - write sample to file
 * @param image  image for sample
 * @param offset  (file) offset to write to
 * @param count  raw counter value
 *
 * Add to the count stored at position @offset in the
 * image file. Overflow pins the count at the maximum
 * value.
 *
 * @count is the raw value passed from the kernel.
 */
void opd_put_image_sample(struct opd_image * image, u32 offset, u16 count)
{
	db_tree_t * sample_file;
	int counter;

	counter = opd_get_counter(count);
	sample_file = &image->sample_files[counter];

	if (!sample_file->base_memory) {
		opd_open_sample_file(image, counter);
		if (!sample_file->base_memory) {
			/* opd_open_sample_file output an error message */
			return;
		}
	}

	db_insert(sample_file, offset, opd_get_count(count));
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

	unsigned int i;
	struct opd_proc * proc;

	opd_stats[OPD_SAMPLES]++;
	opd_stats[OPD_SAMPLE_COUNTS] += opd_get_count(sample->count);

	verbprintf("DO_PUT_SAMPLE: c%d, EIP 0x%.8x, pid %.6d, count %.6d\n",
		opd_get_counter(sample->count), sample->eip, sample->pid, sample->count);

	if (opd_eip_is_kernel(sample->eip)) {
		opd_handle_kernel_sample(sample->eip, sample->count);
		return;
	}

	if (kernel_only)
		return;

	if (!(proc = opd_get_proc(sample->pid))) {
		verbprintf("No proc info for pid %.6d.\n", sample->pid);
		opd_stats[OPD_LOST_PROCESS]++;
		return;
	}

	proc->accessed = 1;

	if (!proc->nr_maps)
		goto out;

	/* proc->last_map is always safe as mappings are never deleted except by
	 * things which reset last_map. If last map is the primary image, we use it
	 * anyway (last_map == 0).
	 */
	opd_stats[OPD_MAP_ARRAY_ACCESS]++;
	if (opd_is_in_map(&proc->maps[proc->last_map], sample->eip)) {
		i = proc->last_map;
		if (proc->maps[i].image != NULL) {
			verb_show_sample(opd_map_offset(&proc->maps[i], sample->eip),
				&proc->maps[i], "(LAST MAP)");
			opd_put_image_sample(proc->maps[i].image,
				opd_map_offset(&proc->maps[i], sample->eip), sample->count);
		}

		opd_stats[OPD_PROCESS]++;
		return;
	}

	/* look for which map and find offset. We search backwards in order to prefer
	 * more recent mappings (which means we don't need to intercept munmap)
	 */
	for (i=proc->nr_maps; i > 0; i--) {
		int const map = i - 1;
		if (opd_is_in_map(&proc->maps[map], sample->eip)) {
			u32 offset = opd_map_offset(&proc->maps[map], sample->eip);
			if (proc->maps[map].image != NULL) {
				verb_show_sample(offset, &proc->maps[map], "");
				opd_put_image_sample(proc->maps[map].image, offset, sample->count);
			}
			proc->last_map = map;
			opd_stats[OPD_PROCESS]++;
			return;
		}
		opd_stats[OPD_MAP_ARRAY_DEPTH]++;
	}

out:
	/* couldn't locate it */
	verbprintf("Couldn't find map for pid %.6d, EIP 0x%.8x.\n", sample->pid, sample->eip);
	opd_stats[OPD_LOST_MAP_PROCESS]++;
	return;
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

	verbprintf("DO_FORK: from %d to %d\n", note->pid, note->addr);

	old = opd_get_proc(note->pid);

	/* we can quite easily get a fork() after the execve() because the notifications
	 * are racy. In particular, the fork notification is done on parent return (so we
	 * know the pid), but this will often be after the execve is done by the child.
	 *
	 * So we only create a new setup if it doesn't exist already, allowing
	 * both the clone() and the execve() cases to work.
	 */
	if (opd_get_proc((u16)note->addr))
		return;

	/* eip is actually pid of new process */
	proc = opd_add_proc((u16)note->addr);

	if (!old)
		return;

	/* remove the kernel map and copy over */

	if (proc->maps) free(proc->maps);
	proc->maps = xmalloc(sizeof(struct opd_map) * old->max_nr_maps);
	memcpy(proc->maps,old->maps,sizeof(struct opd_map) * old->nr_maps);
	proc->nr_maps = old->nr_maps;
	proc->max_nr_maps = old->max_nr_maps;
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

	proc = opd_get_proc(note->pid);
	if (proc) {
		proc->dead = 1;
		proc->accessed = 1;
	} else {
		verbprintf("unknown proc %u just exited.\n", note->pid);
	}
}


/**
 * opd_handle_exec - deal with notification of execve()
 * @param pid  pid of execve()d process
 *
 * Drop all mapping information for the process.
 */
void opd_handle_exec(u16 pid)
{
	struct opd_proc * proc;

	verbprintf("DO_EXEC: pid %u\n", pid);

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
	proc = opd_get_proc(pid);
	if (proc)
		opd_kill_maps(proc);
	else
		opd_add_proc(pid);
}


/**
 * opd_proc_cleanup - clean up on exit
 */
void opd_proc_cleanup(void)
{
	uint i;

	for (i=0; i < OPD_MAX_PROC_HASH; i++) {
		struct opd_proc * proc = opd_procs[i];
		struct opd_proc * next;

		while (proc) {
			next = proc->next;
			opd_delete_proc(proc);
			proc=next;
		}
	}

	opd_clear_module_info();
}
