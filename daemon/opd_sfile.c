/**
 * @file daemon/opd_sfile.c
 * Management of sample files
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 */

#include "opd_sfile.h"

#include "opd_trans.h"
#include "opd_kernel.h"
#include "opd_mangling.h"
#include "opd_printf.h"
#include "opd_stats.h"

#include "op_libiberty.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int separate_lib;
extern int separate_kernel;
extern int separate_thread;
extern int separate_cpu;
extern uint op_nr_counters;

#define HASH_SIZE 2048
#define HASH_BITS (HASH_SIZE - 1)

/** All sfiles are hashed into these lists */
static struct list_head hashes[HASH_SIZE];

/** All sfiles are on this list. */
static LIST_HEAD(lru_list);

/* FIXME: can undoubtedly improve this hashing */
/** Hash the transient parameters for lookup. */
static unsigned long
sfile_hash(struct transient const * trans, struct kernel_image * ki)
{
	unsigned long val = 0;
	
	/* cookie meaningless for kernel, shouldn't hash */
	if (trans->in_kernel) {
		val ^= ki->start >> 14;
		val ^= ki->end >> 7;
	} else {
		val ^= trans->cookie >> DCOOKIE_SHIFT;
	}

	if (separate_thread) {
		val ^= trans->tid << 2;
		val ^= trans->tgid << 2;
	}

	if (separate_kernel || (separate_lib && !ki))
		val ^= trans->app_cookie >> (DCOOKIE_SHIFT + 3);

	if (separate_cpu)
		val ^= trans->cpu;

	return val & HASH_BITS;
}


/**
 * Return true if the given sfile matches the current transient
 * parameters.
 */
static int
sfile_match(struct transient const * trans, struct sfile const * sfile,
            struct kernel_image const * ki)
{
	/* this is a simplified check for "is a kernel image" AND
	 * "is the right kernel image". Also handles no-vmlinux
	 * correctly.
	 */
	if (sfile->kernel != ki)
		return 0;

	if (separate_thread) {
		if (trans->tid != sfile->tid || trans->tgid != sfile->tgid)
			return 0;
	}

	if (separate_cpu) {
		if (trans->cpu != sfile->cpu)
			return 0;
	}

	if (separate_kernel || (separate_lib && !ki)) {
		if (trans->app_cookie != sfile->app_cookie)
			return 0;
	}

	/* ignore the cached trans->cookie for kernel images,
	 * it's meaningless and we checked all others already
	 */
	if (trans->in_kernel)
		return 1;

	return trans->cookie == sfile->cookie;
}


/** create a new sfile matching the current transient parameters */
static struct sfile *
create_sfile(struct transient const * trans, struct kernel_image * ki)
{
	size_t i;
	struct sfile * sf;

	sf = xmalloc(sizeof(struct sfile));

	/* The logic here: if we're in the kernel, the cached cookie is
	 * meaningless (though not the app_cookie if separate_kernel)
	 */
	sf->cookie = trans->in_kernel ? INVALID_COOKIE : trans->cookie;
	sf->app_cookie = INVALID_COOKIE;
	sf->tid = (pid_t)-1;
	sf->tgid = (pid_t)-1;
	sf->cpu = 0;
	sf->kernel = ki;

	for (i = 0 ; i < op_nr_counters ; ++i) {
		odb_init(&sf->files[i]);
	}

	if (separate_thread) {
		sf->tid = trans->tid;
		sf->tgid = trans->tgid;
	}

	if (separate_cpu)
		sf->cpu = trans->cpu;

	if (separate_kernel || (separate_lib && !ki))
		sf->app_cookie = trans->app_cookie;

	return sf;
}


struct sfile * sfile_find(struct transient const * trans)
{
	struct sfile * sf;
	struct list_head * pos;
	struct kernel_image * ki = NULL;
	unsigned long hash;

	/* There is a small race where this *can* happen, see
	 * caller of cpu_buffer_reset() in the kernel
	 */
	if (trans->in_kernel == -1) {
		verbprintf("Losing sample at 0x%llx of unknown provenance.\n",
		           trans->pc);
		opd_stats[OPD_NO_CTX]++;
		return NULL;
	}

	/* we might need a kernel image start/end to hash on */
	if (trans->in_kernel) {
		ki = find_kernel_image(trans);
		if (!ki) {
			verbprintf("Lost kernel sample %llx\n", trans->pc);
			opd_stats[OPD_LOST_KERNEL]++;
			return NULL;
		}
	}
		
	hash = sfile_hash(trans, ki);
	list_for_each(pos, &hashes[hash]) {
		sf = list_entry(pos, struct sfile, hash);
		if (sfile_match(trans, sf, ki))
			goto lru;
	}

	sf = create_sfile(trans, ki);

	list_add(&sf->hash, &hashes[hash]);
	list_add(&sf->lru, &lru_list);

lru:
	list_del(&sf->lru);
	list_add_tail(&sf->lru, &lru_list);
	return sf;
}


static samples_odb_t * get_file(struct sfile * sf, uint counter)
{
	if (!sf->files[counter].base_memory)
		opd_open_sample_file(sf, counter);

	/* Error is logged by opd_open_sample_file */
	if (!sf->files[counter].base_memory)
		return NULL;

	return &sf->files[counter];
}


static void verbose_sample(struct sfile * sf, vma_t pc, uint counter)
{
	char const * name = verbose_cookie(sf->cookie);
	char const * app = verbose_cookie(sf->app_cookie);
	verbprintf("Sample at 0x%llx(%u): %s(%llx), app %s(%llx), kernel %s\n",
	           pc, counter, name, sf->cookie, app, sf->app_cookie,
		   sf->kernel ? sf->kernel->name : "no");
}


void sfile_log_sample(struct sfile * sf, vma_t pc, uint counter)
{
	int err;
	samples_odb_t * file = get_file(sf, counter);

	/* absolute value -> offset */
	if (sf->kernel)
		pc -= sf->kernel->start;

	if (verbose)
		verbose_sample(sf, pc, counter);

	if (!file)
		return;

	opd_stats[OPD_SAMPLES]++;
	opd_stats[sf->kernel ? OPD_KERNEL : OPD_PROCESS]++;

	/* Possible narrowing to 32-bit value only. */
	err = odb_insert(file, (unsigned long)pc, 1);
	if (err) {
		fprintf(stderr, "%s\n", strerror(err));
		abort();
	}
}


static void kill_sfile(struct sfile * sf)
{
	size_t i;
	/* it's OK to close a non-open odb file */
	for (i = 0; i < op_nr_counters; ++i)
		odb_close(&sf->files[i]);
	list_del(&sf->lru);
	list_del(&sf->hash);
	free(sf);
}


void sfile_clear_kernel(void)
{
	struct list_head * pos;
	struct list_head * pos2;
	struct sfile * sf;

	list_for_each_safe(pos, pos2, &lru_list) {
		sf = list_entry(pos, struct sfile, lru);
		if (sf->kernel)
			kill_sfile(sf);
	}
}


void sfile_sync_files(void)
{
	struct list_head * pos;
	struct sfile * sf;
	size_t i;

	list_for_each(pos, &lru_list) {
		sf = list_entry(pos, struct sfile, lru);
		for (i = 0; i < op_nr_counters; ++i)
			odb_sync(&sf->files[i]);
	}
}


void sfile_close_files(void)
{
	struct list_head * pos;
	struct sfile * sf;
	size_t i;

	list_for_each(pos, &lru_list) {
		sf = list_entry(pos, struct sfile, lru);
		for (i = 0; i < op_nr_counters; ++i)
			odb_close(&sf->files[i]);
	}
}


/* this value probably doesn't matter too much */
#define LRU_AMOUNT 1000
int sfile_lru_clear(void)
{
	struct list_head * pos;
	struct list_head * pos2;
	struct sfile * sf;
	int amount = LRU_AMOUNT;

	if (list_empty(&lru_list))
		return 1;

	list_for_each_safe(pos, pos2, &lru_list) {
		if (!--amount)
			break;
		sf = list_entry(pos, struct sfile, lru);
		kill_sfile(sf);
	}

	return 0;
}


void sfile_get(struct sfile * sf)
{
	list_del(&sf->lru);
}


void sfile_put(struct sfile * sf)
{
	list_add_tail(&sf->lru, &lru_list);
}


void sfile_init(void)
{
	size_t i = 0;

	for (; i < HASH_SIZE; ++i)
		list_init(&hashes[i]);
}
