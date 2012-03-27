/**
 * @file pe_profiling/operf_sfile.cpp
 * Management of sample files generated from operf
 * This file is modeled after daemon/opd_sfile.c
 *
 * @remark Copyright 2011 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Maynard Johnson
 * (C) Copyright IBM Corporation 2011
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>

#include "operf_sfile.h"
#include "operf_kernel.h"
#include "operf_utils.h"
#include "cverb.h"
#include "op_string.h"
#include "operf_mangling.h"


// TODO: handle stats
//#include "opd_stats.h"
#include "op_libiberty.h"

#define HASH_SIZE 2048
#define HASH_BITS (HASH_SIZE - 1)

/** All sfiles are hashed into these lists */
static struct list_head hashes[HASH_SIZE];

/** All sfiles are on this list. */
static LIST_HEAD(lru_list);


static unsigned long
sfile_hash(struct operf_transient const * trans, struct operf_kernel_image * ki)
{
	unsigned long val = 0;
	size_t fname_len = strlen(trans->image_name);

	/* fname_ptr will point at the first character in the image name
	 * which we'll use for hashing.  We don't need to hash on the whole
	 * image name to get a decent hash.  Arbitrarily, we'll hash on the
	 * first 16 chars or fname_len (if fname_len < 16).
	 */
	unsigned int fname_hash_len = (fname_len < 16) ? fname_len : 16;
	const char * fname_ptr = trans->image_name + fname_len - fname_hash_len;

	val ^= trans->tid << 2;
	val ^= trans->tgid << 2;

	if (operf_options::separate_cpu)
		val ^= trans->cpu;

	if (trans->in_kernel) {
		val ^= ki->start >> 14;
		val ^= ki->end >> 7;
	}
	for (unsigned int i = 0; i < fname_hash_len; i++)
		val = ((val << 5) + val) ^ fname_ptr[i];

	val ^= trans->tgid << 2;

	// TODO: replace anon
	/*
	if (trans->anon) {
		val ^= trans->anon->start >> VMA_SHIFT;
		val ^= trans->anon->end >> (VMA_SHIFT + 1);
	}
	*/

	return val & HASH_BITS;
}


static int
do_match(struct operf_sfile const * sf, struct operf_kernel_image const * ki,
         //trans->anon,
         const char * image_name, const char * appname, pid_t tgid, pid_t tid, unsigned int cpu)
{
	size_t len1, len2;

	/* this is a simplified check for "is a kernel image" AND
	 * "is the right kernel image". Also handles no-vmlinux
	 * correctly.
	 */
	if (sf->kernel != ki)
		return 0;

	len1 = strlen(sf->app_filename);
	len2 = strlen(appname);
	if ((len1 != len2) || strncmp(sf->app_filename, appname, len1))
		return 0;

	if (sf->tid != tid || sf->tgid != tgid)
		return 0;

	if (operf_options::separate_cpu) {
		if (sf->cpu != cpu)
			return 0;
	}

	if (ki)
		return 1;

	// TODO: handle anon
	/*
	if (sf->anon != anon)
		return 0;
	 */

	return (!strncmp(sf->image_name, image_name, len1));

}

static int
trans_match(struct operf_transient const * trans, struct operf_sfile const * sfile,
            struct operf_kernel_image const * ki)
{
	//TODO  anon replacement
	return do_match(sfile, ki,
	                //trans->anon,
	                trans->image_name, trans->app_filename,
	                trans->tgid, trans->tid, trans->cpu);

}


int
operf_sfile_equal(struct operf_sfile const * sf, struct operf_sfile const * sf2)
{
	//TODO anon replacement
	return do_match(sf, sf2->kernel,
	                //sf2->anon,
	                sf2->image_name, sf2->app_filename,
	                sf2->tgid, sf2->tid, sf2->cpu);
}


/** create a new sfile matching the current transient parameters */
static struct operf_sfile *
create_sfile(unsigned long hash, struct operf_transient const * trans,
             struct operf_kernel_image * ki)
{
	size_t i;
	struct operf_sfile * sf;

	sf = (operf_sfile *)xmalloc(sizeof(struct operf_sfile));

	sf->hashval = hash;

	sf->tid = trans->tid;
	sf->tgid = trans->tgid;
	sf->cpu = 0;
	sf->kernel = ki;
	sf->image_name = trans->image_name;
	sf->app_filename = trans->app_filename;
	// TODO: handle anon
	//sf->anon = trans->anon;

	for (i = 0 ; i < op_nr_counters ; ++i)
		odb_init(&sf->files[i]);

	// TODO:  handle extended
	/*
	if (trans->ext)
		opd_ext_operf_sfile_create(sf);
	else
		*/
	sf->ext_files = NULL;

	for (i = 0; i < CG_HASH_SIZE; ++i)
		list_init(&sf->cg_hash[i]);

	if (operf_options::separate_cpu)
		sf->cpu = trans->cpu;

	return sf;
}

struct operf_sfile * operf_sfile_find(struct operf_transient const * trans)
{
	struct operf_sfile * sf;
	struct list_head * pos;
	struct operf_kernel_image * ki = NULL;
	unsigned long hash;


	if (trans->in_kernel) {
		ki = operf_find_kernel_image(trans->pc);
		/* TODO: handle lost kernel samples */
		if (!ki) {
			cverb << vsfile << "Lost kernel sample " << std::hex << trans->pc << std::endl;;
			//opd_stats[OPD_LOST_KERNEL]++;
			return NULL;
		}
	}

	//TODO anon replacement
	/*
	else if (trans->cookie == NO_COOKIE && !trans->anon) {
		if (vsamples) {
			char const * app = verbose_cookie(trans->app_cookie);
			printf("No anon map for pc %llx, app %s.\n",
			       trans->pc, app);
		}
		opd_stats[OPD_LOST_NO_MAPPING]++;
		return NULL;
	}
	*/
	hash = sfile_hash(trans, ki);
	list_for_each(pos, &hashes[hash]) {
		sf = list_entry(pos, struct operf_sfile, hash);
		if (trans_match(trans, sf, ki)) {
			operf_sfile_get(sf);
			goto lru;
		}
	}
	sf = create_sfile(hash, trans, ki);
	list_add(&sf->hash, &hashes[hash]);


lru:
	operf_sfile_put(sf);
	return sf;
}


void operf_sfile_dup(struct operf_sfile * to, struct operf_sfile * from)
{
	size_t i;

	memcpy(to, from, sizeof (struct operf_sfile));

	for (i = 0 ; i < op_nr_counters ; ++i)
		odb_init(&to->files[i]);

	// TODO: handle extended
	//opd_ext_operf_sfile_dup(to, from);

	for (i = 0; i < CG_HASH_SIZE; ++i)
		list_init(&to->cg_hash[i]);

	list_init(&to->hash);
	list_init(&to->lru);
}


static odb_t * get_file(struct operf_transient const * trans, int is_cg)
{
	struct operf_sfile * sf = trans->current;
	struct operf_sfile * last = trans->last;
	struct operf_cg_entry * cg;
	struct list_head * pos;
	unsigned long hash;
	odb_t * file;

	// TODO: handle extended
	/*
	if ((trans->ext) != NULL)
		return opd_ext_operf_sfile_get(trans, is_cg);
	 */

	if (trans->event >= (int)op_nr_counters) {
		fprintf(stderr, "%s: Invalid counter %d\n", __FUNCTION__,
			trans->event);
		abort();
	}

	file = &sf->files[trans->event];

	if (!is_cg)
		goto open;

	hash = last->hashval & (CG_HASH_SIZE - 1);

	/* Need to look for the right 'to'. Since we're looking for
	 * 'last', we use its hash.
	 */
	list_for_each(pos, &sf->cg_hash[hash]) {
		cg = list_entry(pos, struct operf_cg_entry, hash);
		if (operf_sfile_equal(last, &cg->to)) {
			file = &cg->to.files[trans->event];
			goto open;
		}
	}

	cg = (operf_cg_entry *)xmalloc(sizeof(struct operf_cg_entry));
	operf_sfile_dup(&cg->to, last);
	list_add(&cg->hash, &sf->cg_hash[hash]);
	file = &cg->to.files[trans->event];

open:
	if (!odb_open_count(file))
		operf_open_sample_file(file, last, sf, trans->event, is_cg);

	/* Error is logged by opd_open_sample_file */
	if (!odb_open_count(file))
		return NULL;

	return file;
}


static void verbose_print_sample(struct operf_sfile * sf, vma_t pc, uint counter)
{
	//TODO cookie and anon replacement
//char const * app = verbose_cookie(sf->app_cookie);
	printf("0x%llx(%u): ", pc, counter);
	/*
	if (sf->anon) {
		printf("anon (tgid %u, 0x%llx-0x%llx), ",
		       (unsigned int)sf->anon->tgid,
		       sf->anon->start, sf->anon->end);
	}
	*/
	// TODO: handle kernel
	/*	else if (sf->kernel) {
		printf("kern (name %s, 0x%llx-0x%llx), ", sf->kernel->name,
		       sf->kernel->start, sf->kernel->end);
	} else {
		//TODO cookie replacement
		;
		printf("%s(%llx), ", verbose_cookie(sf->cookie),  sf->cookie);
	}
	*/
	//TODO cookie replacement
	//printf("app %s(%llx)", app, sf->app_cookie);
}


static void verbose_sample(struct operf_transient const * trans, vma_t pc)
{
	printf("Sample ");
	verbose_print_sample(trans->current, pc, trans->event);
	printf("\n");
}


void operf_sfile_log_sample(struct operf_transient const * trans)
{
	operf_sfile_log_sample_count(trans, 1);
}


void operf_sfile_log_sample_count(struct operf_transient const * trans,
                            unsigned long int count)
{
	int err;
	vma_t pc = trans->pc;
	odb_t * file;

	file = get_file(trans, 0);

	/* absolute value -> offset */
	if (trans->current->kernel)
		pc -= trans->current->kernel->start;
// TODO handle anon
	/*
	if (trans->current->anon)
		pc -= trans->current->anon->start;
	 */
	if (cverb << vsfile)
		verbose_sample(trans, pc);
	// TODO: handle stats
	/*
	if (!file) {
		opd_stats[OPD_LOST_SAMPLEFILE]++;
		return;
	}
	 */
	err = odb_update_node_with_offset(file,
					  (odb_key_t)pc,
					  count);
	if (err) {
		fprintf(stderr, "%s: %s\n", __FUNCTION__, strerror(err));
		abort();
	}
}


static int close_sfile(struct operf_sfile * sf, void * data __attribute__((unused)))
{
	size_t i;

	/* it's OK to close a non-open odb file */
	for (i = 0; i < op_nr_counters; ++i)
		odb_close(&sf->files[i]);

	// TODO: handle extended
	//opd_ext_operf_sfile_close(sf);

	return 0;
}


static void kill_sfile(struct operf_sfile * sf)
{
	close_sfile(sf, NULL);
	list_del(&sf->hash);
	list_del(&sf->lru);
}


static int sync_sfile(struct operf_sfile * sf, void * data __attribute__((unused)))
{
	size_t i;

	for (i = 0; i < op_nr_counters; ++i)
		odb_sync(&sf->files[i]);

	// TODO: handle extended
	//opd_ext_operf_sfile_sync(sf);

	return 0;
}


static int is_sfile_kernel(struct operf_sfile * sf, void * data __attribute__((unused)))
{
	return !!sf->kernel;
}

/* TODO handle anon
static int is_sfile_anon(struct operf_sfile * sf, void * data)
{
	return sf->anon == data;
}

*/

typedef int (*operf_sfile_func)(struct operf_sfile *, void *);

static void
for_one_sfile(struct operf_sfile * sf, operf_sfile_func func, void * data)
{
	size_t i;
	int free_sf = func(sf, data);

	for (i = 0; i < CG_HASH_SIZE; ++i) {
		struct list_head * pos;
		struct list_head * pos2;
		list_for_each_safe(pos, pos2, &sf->cg_hash[i]) {
			struct operf_cg_entry * cg =
				list_entry(pos, struct operf_cg_entry, hash);
			if (free_sf || func(&cg->to, data)) {
				kill_sfile(&cg->to);
				list_del(&cg->hash);
				free(cg);
			}
		}
	}

	if (free_sf) {
		kill_sfile(sf);
		free(sf);
	}
}


static void for_each_sfile(operf_sfile_func func, void * data)
{
	struct list_head * pos;
	struct list_head * pos2;

	list_for_each_safe(pos, pos2, &lru_list) {
		struct operf_sfile * sf = list_entry(pos, struct operf_sfile, lru);
		for_one_sfile(sf, func, data);
	}
}


void operf_sfile_clear_kernel(void)
{
	for_each_sfile(is_sfile_kernel, NULL);
}


void operf_sfile_clear_anon(struct anon_mapping * anon)
{
	//for_each_sfile(is_sfile_anon, anon);
}


void operf_sfile_sync_files(void)
{
	for_each_sfile(sync_sfile, NULL);
}


void operf_sfile_close_files(void)
{
	for_each_sfile(close_sfile, NULL);
}


static int always_true(void)
{
	return 1;
}


#define LRU_AMOUNT 256

/*
 * Clear out older sfiles. Note the current sfiles we're using
 * will not be present in this list, due to operf_sfile_get/put() pairs
 * around the caller of this.
 */
int operf_sfile_lru_clear(void)
{
	struct list_head * pos;
	struct list_head * pos2;
	int amount = LRU_AMOUNT;

	if (list_empty(&lru_list))
		return 1;

	list_for_each_safe(pos, pos2, &lru_list) {
		struct operf_sfile * sf;
		if (!--amount)
			break;
		sf = list_entry(pos, struct operf_sfile, lru);
		for_one_sfile(sf, (operf_sfile_func)always_true, NULL);
	}

	return 0;
}


void operf_sfile_get(struct operf_sfile * sf)
{
	if (sf)
		list_del(&sf->lru);
}


void operf_sfile_put(struct operf_sfile * sf)
{
	if (sf)
		list_add_tail(&sf->lru, &lru_list);
}


void operf_sfile_init(void)
{
	size_t i = 0;

	for (; i < HASH_SIZE; ++i)
		list_init(&hashes[i]);
}
