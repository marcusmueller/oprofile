/**
 * @file daemon/opd_sfile.c
 * Management of sample files
 *
 * @remark Copyright 2002, 2005 OProfile authors
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
#include "oprofiled.h"

#include "op_libiberty.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

	for (i = 0 ; i < op_nr_counters ; ++i)
		odb_init(&sf->files[i]);

	for (i = 0; i < CG_HASH_TABLE_SIZE; ++i)
		list_init(&sf->cg_files[i]);

	if (separate_thread) {
		sf->tid = trans->tid;
		sf->tgid = trans->tgid;
	}

	if (separate_cpu)
		sf->cpu = trans->cpu;

	if (separate_kernel || (separate_lib && !ki))
		sf->app_cookie = trans->app_cookie;

	if (!ki)
		sf->ignored = is_cookie_ignored(sf->cookie);
	else
		sf->ignored = is_image_ignored(ki->name);

	/* give a dependent sfile a chance to redeem itself */
	if (sf->ignored && sf->app_cookie != INVALID_COOKIE)
		sf->ignored = is_cookie_ignored(sf->app_cookie);

	return sf;
}


struct sfile * sfile_find(struct transient const * trans)
{
	struct sfile * sf;
	struct list_head * pos;
	struct kernel_image * ki = NULL;
	unsigned long hash;

	if (trans->tracing != TRACING_ON) {
		opd_stats[OPD_SAMPLES]++;
		opd_stats[trans->in_kernel == 1 ? OPD_KERNEL : OPD_PROCESS]++;
	}

	/* There is a small race where this *can* happen, see
	 * caller of cpu_buffer_reset() in the kernel
	 */
	if (trans->in_kernel == -1) {
		verbprintf(vsamples, "Losing sample at 0x%llx of unknown provenance.\n",
		           trans->pc);
		opd_stats[OPD_NO_CTX]++;
		return NULL;
	}

	/* we might need a kernel image start/end to hash on */
	if (trans->in_kernel) {
		ki = find_kernel_image(trans);
		if (!ki) {
			verbprintf(vsamples, "Lost kernel sample %llx\n", trans->pc);
			opd_stats[OPD_LOST_KERNEL]++;
			return NULL;
		}
	}

	if (trans->cookie == NO_COOKIE) {
		verbprintf(vsamples, "No permanent mapping for pc 0x%llx.\n",
		           trans->pc);
		opd_stats[OPD_LOST_NO_MAPPING]++;
		return NULL;
	}

	hash = sfile_hash(trans, ki);
	list_for_each(pos, &hashes[hash]) {
		sf = list_entry(pos, struct sfile, hash);
		if (sfile_match(trans, sf, ki)) {
			sfile_get(sf);
			goto lru;
		}
	}

	sf = create_sfile(trans, ki);
	list_add(&sf->hash, &hashes[hash]);

lru:
	sfile_put(sf);
	return sf;
}

static void init_cg_id(cg_id * id, struct sfile const * sf)
{
	id->cookie = sf->cookie;
	id->start = 0;
	id->end = (vma_t)-1;
	if (sf->kernel) {
		id->start = sf->kernel->start;
		id->end = sf->kernel->end;
	}
}


static size_t cg_hash(cookie_t from, cookie_t to, size_t counter)
{
	/* FIXME: better hash ? */
	return ((from >> 32) ^ from ^ (to >> 32) ^ to ^ counter) % CG_HASH_TABLE_SIZE;
}


static odb_t *
get_file(struct sfile * sf, struct sfile * last, uint counter, int cg,
   vma_t from, vma_t to)
{
	odb_t * file;

	if (counter >= op_nr_counters) {
		fprintf(stderr, "%s: Invalid counter %u\n", __FUNCTION__,
			counter);
		abort();
	}

	file = &sf->files[counter];

	if (cg) {
		struct cg_hash_entry * temp;
		size_t hash = cg_hash(last->cookie, sf->cookie, counter);
		struct list_head * pos;
		list_for_each(pos, &sf->cg_files[hash]) {
			temp = list_entry(pos, struct cg_hash_entry, next);
			if (temp->from.cookie == sf->cookie &&
			    temp->to.cookie == last->cookie &&
			    from >= temp->from.start && from < temp->from.end &&
			    to >= temp->to.start && to < temp->to.end &&
			    temp->counter == counter)
				break;
		}

		if (pos == &sf->cg_files[hash]) {
			temp = xmalloc(sizeof(struct cg_hash_entry));
			odb_init(&temp->file);
			init_cg_id(&temp->from, sf);
			init_cg_id(&temp->to, last);
			verbprintf(vsamples, "new cg : (%llx, %llx) --> (%llx, %llx) %llx --> %llx, (%llx %llx)\n",
			       temp->from.start, temp->from.end, temp->to.start, temp->to.end,
			       from, to, temp->from.cookie, temp->to.cookie);
			temp->counter = counter;
			list_add(&temp->next, &sf->cg_files[hash]);
		} else {
			temp = list_entry(pos, struct cg_hash_entry, next);
		}

		file = &temp->file;
	}

	if (!odb_open_count(file))
		opd_open_sample_file(file, last, sf, counter, cg);

	/* Error is logged by opd_open_sample_file */
	if (!odb_open_count(file))
		return NULL;

	return file;
}


static void
verbose_sample(char const * prefix, struct sfile * sf, vma_t pc, uint counter)
{
	char const * name = verbose_cookie(sf->cookie);
	char const * app = verbose_cookie(sf->app_cookie);
	printf("%s at 0x%llx(%u): %s(%llx), app %s(%llx), kernel %s\n",
	       prefix, pc, counter, name, sf->cookie, app, sf->app_cookie,
	       sf->kernel ? sf->kernel->name : "no");
}


static void sfile_log_arc(struct transient const * trans)
{
	int err;
	vma_t from = trans->pc;
	vma_t to = trans->last_pc;
	uint64_t key;
	odb_t * file;

	file = get_file(trans->current, trans->last, trans->event, 1, from, to);

	/* absolute value -> offset */
	if (trans->current->kernel)
		from -= trans->current->kernel->start;

	if (trans->last->kernel)
		to -= trans->last->kernel->start;

	if (varcs)
		verbose_sample("Arc", trans->current, to, 0);

	if (!file) {
		opd_stats[OPD_LOST_SAMPLEFILE]++;
		return;
	}

	/* Possible narrowings to 32-bit value only. */
	key = to & (0xffffffff);
	key |= ((uint64_t)from) << 32;

	err = odb_insert(file, key, 1);
	if (err) {
		fprintf(stderr, "%s: %s\n", __FUNCTION__, strerror(err));
		abort();
	}
}


void sfile_log_sample(struct transient const * trans)
{
	int err;
	vma_t pc = trans->pc;
	odb_t * file;

	if (trans->tracing == TRACING_ON) {
		/* can happen if kernel sample falls through the cracks,
		 * see opd_put_sample() */
		if (trans->last)
			sfile_log_arc(trans);
		return;
	}

	file = get_file(trans->current, trans->last, trans->event, 0, 0, 0);

	/* absolute value -> offset */
	if (trans->current->kernel)
		pc -= trans->current->kernel->start;

	if (vsamples)
		verbose_sample("Sample", trans->current, pc, trans->event);

	if (!file) {
		opd_stats[OPD_LOST_SAMPLEFILE]++;
		return;
	}

	err = odb_insert(file, (uint64_t)pc, 1);
	if (err) {
		fprintf(stderr, "%s: %s\n", __FUNCTION__, strerror(err));
		abort();
	}
}


static void kill_cg_file(struct cg_hash_entry * cg)
{
	odb_close(&cg->file);
	list_del(&cg->next);
	free(cg);
}


static void kill_sfile(struct sfile * sf)
{
	size_t i;

	/* it's OK to close a non-open odb file */
	for (i = 0; i < op_nr_counters; ++i)
		odb_close(&sf->files[i]);

	for (i = 0 ; i < CG_HASH_TABLE_SIZE; ++i) {
		struct list_head * pos, * pos2;
		list_for_each_safe(pos, pos2, &sf->cg_files[i]) {
			struct cg_hash_entry * cg = 
				list_entry(pos, struct cg_hash_entry, next);
			kill_cg_file(cg);
		}
	}

	list_del(&sf->lru);
	list_del(&sf->hash);
	free(sf);
}


void sfile_clear_kernel(void)
{
	struct list_head * pos;
	struct list_head * pos2;

	list_for_each_safe(pos, pos2, &lru_list) {
		size_t i;
		struct sfile * sf = list_entry(pos, struct sfile, lru);
		if (sf->kernel) {
			kill_sfile(sf);
			continue;
		}

		for (i = 0 ; i < CG_HASH_TABLE_SIZE; ++i) {
			struct list_head * pos, * pos2;
			list_for_each_safe(pos, pos2, &sf->cg_files[i]) {
				struct cg_hash_entry * cg = list_entry(pos,
					struct cg_hash_entry, next);
				if (cg->from.start || cg->to.start)
					kill_cg_file(cg);
			}
		}
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

		for (i = 0 ; i < CG_HASH_TABLE_SIZE; ++i) {
			struct list_head * pos;
			list_for_each(pos, &sf->cg_files[i]) {
				struct cg_hash_entry * temp = list_entry(pos,
					struct cg_hash_entry, next);
				odb_sync(&temp->file);
			}
		}
	}
}


void sfile_close_files(void)
{
	struct list_head * pos;
	struct sfile * sf;
	size_t i;

	list_for_each(pos, &lru_list) {
		sf = list_entry(pos, struct sfile, lru);
		for (i = 0; i < op_nr_counters; ++i) {
			odb_close(&sf->files[i]);
		}

		for (i = 0 ; i < CG_HASH_TABLE_SIZE; ++i) {
			struct list_head * pos;
			list_for_each(pos, &sf->cg_files[i]) {
				struct cg_hash_entry * temp = list_entry(pos,
					struct cg_hash_entry, next);
				odb_close(&temp->file);
			}
		}
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
