/**
 * @file  pe_profiling/operf_sfile.h
 * Management of sample files generated from operf
 * This file is modeled after daemon/opd_sfile.c
 *
 * @remark Copyright 2011 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Maynard Johnson
 * (C) Copyright IBM Corporation 2011
 */

#ifndef OPD_SFILE_H
#define OPD_SFILE_H

#include "operf_utils.h"
#include "odb.h"
#include "op_hw_config.h"
#include "op_types.h"
#include "op_list.h"
#include "operf_process_info.h"

#include <sys/types.h>

struct operf_transient;
struct operf_kernel_image;

#define CG_HASH_SIZE 16
#define INVALID_IMAGE "INVALID IMAGE"

#define VMA_SHIFT 13

/**
 * Each set of sample files (where a set is over the physical counter
 * types) will have one of these for it. We match against the
 * descriptions here to find which sample DB file we need to modify.
 *
 * cg files are stored in the hash.
 */
struct operf_sfile {
	/** hash value for this sfile */
	unsigned long hashval;
	const char * image_name;
	const char * app_filename;
	size_t image_len, app_len;
	/** thread ID, -1 if not set */
	pid_t tid;
	/** thread group ID, -1 if not set */
	pid_t tgid;
	/** CPU number */
	unsigned int cpu;
	/** kernel image if applicable */
	struct operf_kernel_image * kernel;
	bool is_anon;
	vma_t start_addr;
	vma_t end_addr;

	/** hash table link */
	struct list_head hash;
	/** lru list */
	struct list_head lru;
	/** true if this file should be ignored in profiles */
	int ignored;
	/** opened sample files */
	odb_t files[OP_MAX_EVENTS];
	/** extended sample files */
	odb_t * ext_files;
	/** hash table of opened cg sample files */
	struct list_head cg_hash[CG_HASH_SIZE];
};

/** a call-graph entry */
struct operf_cg_entry {
	/** where arc is to */
	struct operf_sfile to;
	/** next in the hash slot */
	struct list_head hash;
};

/**
 * Transient values used for parsing the event buffer.
 * Note that these are reset for each buffer read, but
 * that should be ok as in the kernel, cpu_buffer_reset()
 * ensures that a correct context starts off the buffer.
 */
struct operf_transient {
	struct operf_sfile * current;
	struct operf_sfile * last;
	bool is_anon;
	operf_process_info * cur_procinfo;
	vma_t pc;
	const char * image_name;
	char app_filename[PATH_MAX];
	size_t image_len, app_len;
	vma_t last_pc;
	int event;
	u64 sample_id;
	int in_kernel;
	unsigned long cpu;
	u32 tid;
	u32 tgid;
	vma_t start_addr;
	vma_t end_addr;
	bool cg;
	// TODO: handle extended
	//void * ext;
};


/** clear any sfiles that are for the kernel */
void operf_sfile_clear_kernel(void);

/** sync sample files */
void operf_sfile_sync_files(void);

/** close sample files */
void operf_sfile_close_files(void);

/** clear out a certain amount of LRU entries
 * return non-zero if the lru is already empty */
int operf_sfile_lru_clear(void);

/** remove a sfile from the lru list, protecting it from operf_sfile_lru_clear() */
void operf_sfile_get(struct operf_sfile * sf);

/** add this sfile to lru list */
void operf_sfile_put(struct operf_sfile * sf);

/**
 * Find the sfile for the current parameters. Note that is required
 * that the PC value be set appropriately (needed for kernel images)
 */
struct operf_sfile * operf_sfile_find(struct operf_transient const * trans);

/** Log the sample in a previously located sfile. */
void operf_sfile_log_sample(struct operf_transient const * trans);

/** Log the event/cycle count in a previously located sfile */
void operf_sfile_log_sample_count(struct operf_transient const * trans,
                            unsigned long int count);

/** Log a callgraph arc. */
void operf_sfile_log_arc(struct operf_transient const * trans);

/** initialise hashes */
void operf_sfile_init(void);

#endif /* OPD_SFILE_H */
