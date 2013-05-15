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
#include "opd_anon.h"
#include "opd_printf.h"
#include "opd_stats.h"
#include "opd_extended.h"
#include "oprofiled.h"

#include "op_libiberty.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

#define HASH_SIZE 2048
#define HASH_BITS (HASH_SIZE - 1)

/** All sfiles are hashed into these lists */
static struct list_head hashes[HASH_SIZE];

/* This data structure is used to help us determine when we should
 * discard user context kernel samples for which we no longer have
 * an app name to which we can attribute them.  This can happen (especially
 * on a busy system) in the following scenario:
 *  - A user context switch occurs.
 *  - User context kernel samples are recorded for this process.
 *  - The user process ends.
 *  - The above-mentioned sample information is first recorded into the per-CPU
 *  buffer and later transferred to the main event buffer.  Since the process
 *  for which this context switch was recorded ended before the transfer
 *  occurs, the app cookie that is recorded into the event buffer along with the
 *  CTX_SWITCH_CODE will be set to NO_COOKIE. When the oprofile userspace daemon
 *  processes the CTX_SWITCH_CODE, it sets trans->app_cookie to NO_COOKIE and then
 *  continues to process the kernel samples. But having no appname in order to
 *  locate the appropriate sample file, it creates a new sample file of the form:
 *  <session_dir>/current/{kern}/<some_kernel_image>/{dep}/{kern}/<some_kernel_image>/<event_spec>.<tgid>.<tid>.<cpu>
 *
 *  This is not really an invalid form for sample files, since it is certainly valid for
 *  oprofile to collect samples for kernel threads that are not running in any process context.
 *  Such samples would be stored in sample files like this, and opreport would show those
 *  samples as having an appname of "<some_kernel_image>".  But if the tgid/tid info for
 *  the sample is from a defunct user process, we should discard these samples.  Not doing so
 *  can lead to strange results when generating reports by tgid/tid (i.e., appname of
 *  "<some_kernel_image>" instead of the real app name associated with the given tgid/tid.
 *  The following paragraph describes the technique for identifying and discarding such samples.
 *
 * When processing a kernel sample for which trans->app_cookie==NO_COOKIE, we inspect the
 * /proc/<pid>/cmdline file.  Housekeeping types of kernel threads (e.g., kswapd, watchdog)
 * won't have a command line since they exist and operate outside of a process context.
 * However, other kernel "tasks" do operate within a process context (e.g., some kernel
 * driver functions, kernel functions invoked via a syscall, etc.).  When we get samples
 * for the latter type of task but no longer have app name info for the process for which
 * the kernel task is performing work, we cannot correctly attribute those kernel samples
 * to a user application, so they should be discarded.  We classify the two different types
 * of kernel "tasks" based on whether or not the /proc/<pid>/cmdline is empty.  We cache
 * the results in kernel_cmdlines for fast lookup when processing samples.
 */
static struct list_head kernel_cmdlines[HASH_SIZE];
struct kern_cmdline {
	pid_t kern_pid;
	struct list_head hash;
	unsigned int has_cmdline;
};

/** All sfiles are on this list. */
static LIST_HEAD(lru_list);


/* FIXME: can undoubtedly improve this hashing */
/** Hash the transient parameters for lookup. */
static unsigned long
sfile_hash(struct transient const * trans, struct kernel_image * ki)
{
	unsigned long val = 0;
	
	if (separate_thread) {
		val ^= trans->tid << 2;
		val ^= trans->tgid << 2;
	}

	if (separate_kernel || ((trans->anon || separate_lib) && !ki))
		val ^= trans->app_cookie >> (DCOOKIE_SHIFT + 3);

	if (separate_cpu)
		val ^= trans->cpu;

	/* cookie meaningless for kernel, shouldn't hash */
	if (trans->in_kernel && ki) {
		val ^= ki->start >> 14;
		val ^= ki->end >> 7;
		return val & HASH_BITS;
	}

	if (trans->cookie != NO_COOKIE) {
		val ^= trans->cookie >> DCOOKIE_SHIFT;
		return val & HASH_BITS;
	}

	if (!separate_thread)
		val ^= trans->tgid << 2;

	if (trans->anon) {
		val ^= trans->anon->start >> VMA_SHIFT;
		val ^= trans->anon->end >> (VMA_SHIFT + 1);
	}

	return val & HASH_BITS;
}


static int
do_match(struct sfile const * sf, cookie_t cookie, cookie_t app_cookie,
         struct kernel_image const * ki, struct anon_mapping const * anon,
         pid_t tgid, pid_t tid, unsigned int cpu)
{
	/* this is a simplified check for "is a kernel image" AND
	 * "is the right kernel image". Also handles no-vmlinux
	 * correctly.
	 */
	if (sf->kernel != ki)
		return 0;

	if (separate_thread) {
		if (sf->tid != tid || sf->tgid != tgid)
			return 0;
	}

	if (separate_cpu) {
		if (sf->cpu != cpu)
			return 0;
	}

	if (separate_kernel || ((anon || separate_lib) && !ki)) {
		if (sf->app_cookie != app_cookie)
			return 0;
	}

	/* ignore the cached trans->cookie for kernel images,
	 * it's meaningless and we checked all others already
	 */
	if (ki)
		return 1;

	if (sf->anon != anon)
		return 0;

	return sf->cookie == cookie;
}


static int
trans_match(struct transient const * trans, struct sfile const * sfile,
            struct kernel_image const * ki)
{
	return do_match(sfile, trans->cookie, trans->app_cookie, ki,
	                trans->anon, trans->tgid, trans->tid, trans->cpu);
}


int
sfile_equal(struct sfile const * sf, struct sfile const * sf2)
{
	return do_match(sf, sf2->cookie, sf2->app_cookie, sf2->kernel,
	                sf2->anon, sf2->tgid, sf2->tid, sf2->cpu);
}


static int
is_sf_ignored(struct sfile const * sf)
{
	if (sf->kernel) {
		if (!is_image_ignored(sf->kernel->name))
			return 0;

		/* Let a dependent kernel image redeem the sf if we're
		 * executing on behalf of an application.
		 */
		return is_cookie_ignored(sf->app_cookie);
	}

	/* Anon regions are always dependent on the application.
 	 * Otherwise, let a dependent image redeem the sf.
	 */
	if (sf->anon || is_cookie_ignored(sf->cookie))
		return is_cookie_ignored(sf->app_cookie);

	return 0;
}


/** create a new sfile matching the current transient parameters */
static struct sfile *
create_sfile(unsigned long hash, struct transient const * trans,
             struct kernel_image * ki)
{
	size_t i;
	struct sfile * sf;

	sf = xmalloc(sizeof(struct sfile));

	sf->hashval = hash;

	/* The logic here: if we're in the kernel, the cached cookie is
	 * meaningless (though not the app_cookie if separate_kernel)
	 */
	sf->cookie = trans->in_kernel ? INVALID_COOKIE : trans->cookie;
	sf->app_cookie = INVALID_COOKIE;
	sf->tid = (pid_t)-1;
	sf->tgid = (pid_t)-1;
	sf->cpu = 0;
	sf->kernel = ki;
	sf->anon = trans->anon;

	for (i = 0 ; i < op_nr_counters ; ++i)
		odb_init(&sf->files[i]);

	if (trans->ext)
		opd_ext_sfile_create(sf);
	else
		sf->ext_files = NULL;

	for (i = 0; i < CG_HASH_SIZE; ++i)
		list_init(&sf->cg_hash[i]);

	if (separate_thread)
		sf->tid = trans->tid;
	if (separate_thread || trans->cookie == NO_COOKIE)
		sf->tgid = trans->tgid;

	if (separate_cpu)
		sf->cpu = trans->cpu;

	if (separate_kernel || ((trans->anon || separate_lib) && !ki))
		sf->app_cookie = trans->app_cookie;

	sf->ignored = is_sf_ignored(sf);

	sf->embedded_offset = trans->embedded_offset;

	/* If embedded_offset is a valid value, it means we're
	 * processing a Cell BE SPU profile; in which case, we
	 * want sf->app_cookie to hold trans->app_cookie.
	 */
	if (trans->embedded_offset != UNUSED_EMBEDDED_OFFSET)
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
		// We *know* that PID 0, 1, and 2 are pure kernel context tasks, so
		// we always want to keep these samples.
		if ((trans->tgid == 0) || (trans->tgid == 1) || (trans->tgid == 2))
			goto find_sfile;

		// Decide whether or not this kernel sample should be discarded.
		// See detailed description above where the kernel_cmdlines hash
		// table is defined.
		if (trans->app_cookie == NO_COOKIE) {
			int found = 0;
			struct kern_cmdline * kcmd;
			hash = (trans->tgid << 2) & HASH_BITS;
			list_for_each(pos, &kernel_cmdlines[hash]) {
				kcmd = list_entry(pos, struct kern_cmdline, hash);
				if (kcmd->kern_pid == trans->tgid) {
					found = 1;
					if (kcmd->has_cmdline) {
						verbprintf(vsamples,
						           "Dropping user context kernel sample 0x%llx "
						           "for process %u due to no app cookie available.\n",
						           (unsigned long long)trans->pc, trans->tgid);
						opd_stats[OPD_NO_APP_KERNEL_SAMPLE]++;
						return NULL;
					}
					break;
				}
			}
			if (!found) {
				char name[32], dst[8];
				int fd, dropped = 0;
				kcmd = (struct kern_cmdline *)xmalloc(sizeof(*kcmd));
				kcmd->kern_pid = trans->tgid;
				snprintf(name, sizeof name, "/proc/%u/cmdline", trans->tgid);
				fd = open(name, O_RDONLY);
				if(fd==-1) {
					// Most likely due to process ending, so we'll assume it used to have a cmdline
					kcmd->has_cmdline = 1;
					verbprintf(vsamples,
					           "Open of /proc/%u/cmdline failed, so dropping "
					           "kernel sameple 0x%llx\n",
					           trans->tgid, (unsigned long long)trans->pc);
					opd_stats[OPD_NO_APP_KERNEL_SAMPLE]++;
					dropped = 1;
				} else {
					if((read(fd, dst, 8) < 1)) {
						verbprintf(vsamples, "No cmdline for PID %u\n", trans->tgid);
						kcmd->has_cmdline = 0;
					} else {
						// This *really* shouldn't happen.  If it does, then why don't
						// we have an app_cookie?
						dst[7] = '\0';
						verbprintf(vsamples, "Start of cmdline for PID %u is %s\n", trans->tgid, dst);
						kcmd->has_cmdline = 1;
						opd_stats[OPD_NO_APP_KERNEL_SAMPLE]++;
						dropped = 1;
					}
					close(fd);
				}
				list_add(&kcmd->hash, &kernel_cmdlines[hash]);
				if (dropped)
					return NULL;
			}
		}
	} else if (trans->cookie == NO_COOKIE && !trans->anon) {
		if (vsamples) {
			char const * app = verbose_cookie(trans->app_cookie);
			printf("No anon map for pc %llx, app %s.\n",
			       trans->pc, app);
		}
		opd_stats[OPD_LOST_NO_MAPPING]++;
		return NULL;
	}

find_sfile:
	hash = sfile_hash(trans, ki);
	list_for_each(pos, &hashes[hash]) {
		sf = list_entry(pos, struct sfile, hash);
		if (trans_match(trans, sf, ki)) {
			sfile_get(sf);
			goto lru;
		}
	}

	sf = create_sfile(hash, trans, ki);
	list_add(&sf->hash, &hashes[hash]);

lru:
	sfile_put(sf);
	return sf;
}


void sfile_dup(struct sfile * to, struct sfile * from)
{
	size_t i;

	memcpy(to, from, sizeof (struct sfile));

	for (i = 0 ; i < op_nr_counters ; ++i)
		odb_init(&to->files[i]);

	opd_ext_sfile_dup(to, from);

	for (i = 0; i < CG_HASH_SIZE; ++i)
		list_init(&to->cg_hash[i]);

	list_init(&to->hash);
	list_init(&to->lru);
}


static odb_t * get_file(struct transient const * trans, int is_cg)
{
	struct sfile * sf = trans->current;
	struct sfile * last = trans->last;
	struct cg_entry * cg;
	struct list_head * pos;
	unsigned long hash;
	odb_t * file;

	if ((trans->ext) != NULL)
		return opd_ext_sfile_get(trans, is_cg);

	if (trans->event >= op_nr_counters) {
		fprintf(stderr, "%s: Invalid counter %lu\n", __FUNCTION__,
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
		cg = list_entry(pos, struct cg_entry, hash);
		if (sfile_equal(last, &cg->to)) {
			file = &cg->to.files[trans->event];
			goto open;
		}
	}

	cg = xmalloc(sizeof(struct cg_entry));
	sfile_dup(&cg->to, last);
	list_add(&cg->hash, &sf->cg_hash[hash]);
	file = &cg->to.files[trans->event];

open:
	if (!odb_open_count(file))
		opd_open_sample_file(file, last, sf, trans->event, is_cg);

	/* Error is logged by opd_open_sample_file */
	if (!odb_open_count(file))
		return NULL;

	return file;
}


static void verbose_print_sample(struct sfile * sf, vma_t pc, uint counter)
{
	char const * app = verbose_cookie(sf->app_cookie);
	printf("0x%llx(%u): ", pc, counter);
	if (sf->anon) {
		printf("anon (tgid %u, 0x%llx-0x%llx), ",
		       (unsigned int)sf->anon->tgid,
		       sf->anon->start, sf->anon->end);
	} else if (sf->kernel) {
		printf("kern (name %s, 0x%llx-0x%llx), ", sf->kernel->name,
		       sf->kernel->start, sf->kernel->end);
	} else {
		printf("%s(%llx), ", verbose_cookie(sf->cookie),  sf->cookie);
	}
	printf("app %s(%llx)", app, sf->app_cookie);
}


static void verbose_sample(struct transient const * trans, vma_t pc)
{
	printf("Sample ");
	verbose_print_sample(trans->current, pc, trans->event);
	printf("\n");
}


static void
verbose_arc(struct transient const * trans, vma_t from, vma_t to)
{
	printf("Arc ");
	verbose_print_sample(trans->current, from, trans->event);
	printf(" -> 0x%llx", to);
	printf("\n");
}


static void sfile_log_arc(struct transient const * trans)
{
	int err;
	vma_t from = trans->pc;
	vma_t to = trans->last_pc;
	uint64_t key;
	odb_t * file;

	file = get_file(trans, 1);

	/* absolute value -> offset */
	if (trans->current->kernel)
		from -= trans->current->kernel->start;

	if (trans->last->kernel)
		to -= trans->last->kernel->start;

	if (trans->current->anon)
		from -= trans->current->anon->start;

	if (trans->last->anon)
		to -= trans->last->anon->start;

	if (varcs)
		verbose_arc(trans, from, to);

	if (!file) {
		opd_stats[OPD_LOST_SAMPLEFILE]++;
		return;
	}

	/* Possible narrowings to 32-bit value only. */
	key = to & (0xffffffff);
	key |= ((uint64_t)from) << 32;

	err = odb_update_node(file, key);
	if (err) {
		fprintf(stderr, "%s: %s\n", __FUNCTION__, strerror(err));
		abort();
	}
}


void sfile_log_sample(struct transient const * trans)
{
	sfile_log_sample_count(trans, 1);
}


void sfile_log_sample_count(struct transient const * trans,
                            unsigned long int count)
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

	file = get_file(trans, 0);

	/* absolute value -> offset */
	if (trans->current->kernel)
		pc -= trans->current->kernel->start;

	if (trans->current->anon)
		pc -= trans->current->anon->start;

	if (vsamples)
		verbose_sample(trans, pc);

	if (!file) {
		opd_stats[OPD_LOST_SAMPLEFILE]++;
		return;
	}

	err = odb_update_node_with_offset(file,
					  (odb_key_t)pc,
					  count);
	if (err) {
		fprintf(stderr, "%s: %s\n", __FUNCTION__, strerror(err));
		abort();
	}
}


static int close_sfile(struct sfile * sf, void * data __attribute__((unused)))
{
	size_t i;

	/* it's OK to close a non-open odb file */
	for (i = 0; i < op_nr_counters; ++i)
		odb_close(&sf->files[i]);

	opd_ext_sfile_close(sf);

	return 0;
}


static void kill_sfile(struct sfile * sf)
{
	close_sfile(sf, NULL);
	list_del(&sf->hash);
	list_del(&sf->lru);
}


static int sync_sfile(struct sfile * sf, void * data __attribute__((unused)))
{
	size_t i;

	for (i = 0; i < op_nr_counters; ++i)
		odb_sync(&sf->files[i]);

	opd_ext_sfile_sync(sf);

	return 0;
}


static int is_sfile_kernel(struct sfile * sf, void * data __attribute__((unused)))
{
	return !!sf->kernel;
}


static int is_sfile_anon(struct sfile * sf, void * data)
{
	return sf->anon == data;
}


typedef int (*sfile_func)(struct sfile *, void *);

static void
for_one_sfile(struct sfile * sf, sfile_func func, void * data)
{
	size_t i;
	int free_sf = func(sf, data);

	for (i = 0; i < CG_HASH_SIZE; ++i) {
		struct list_head * pos;
		struct list_head * pos2;
		list_for_each_safe(pos, pos2, &sf->cg_hash[i]) {
			struct cg_entry * cg =
				list_entry(pos, struct cg_entry, hash);
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


static void for_each_sfile(sfile_func func, void * data)
{
	struct list_head * pos;
	struct list_head * pos2;

	list_for_each_safe(pos, pos2, &lru_list) {
		struct sfile * sf = list_entry(pos, struct sfile, lru);
		for_one_sfile(sf, func, data);
	}
}


void sfile_clear_kernel(void)
{
	for_each_sfile(is_sfile_kernel, NULL);
}


void sfile_clear_anon(struct anon_mapping * anon)
{
	for_each_sfile(is_sfile_anon, anon);
}


void sfile_sync_files(void)
{
	for_each_sfile(sync_sfile, NULL);
}


void sfile_close_files(void)
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
 * will not be present in this list, due to sfile_get/put() pairs
 * around the caller of this.
 */
int sfile_lru_clear(void)
{
	struct list_head * pos;
	struct list_head * pos2;
	int amount = LRU_AMOUNT;

	if (list_empty(&lru_list))
		return 1;

	list_for_each_safe(pos, pos2, &lru_list) {
		struct sfile * sf;
		if (!--amount)
			break;
		sf = list_entry(pos, struct sfile, lru);
		for_one_sfile(sf, (sfile_func)always_true, NULL);
	}

	return 0;
}


void sfile_get(struct sfile * sf)
{
	if (sf)
		list_del(&sf->lru);
}


void sfile_put(struct sfile * sf)
{
	if (sf)
		list_add_tail(&sf->lru, &lru_list);
}


void sfile_init(void)
{
	size_t i = 0;

	for (; i < HASH_SIZE; ++i) {
		list_init(&hashes[i]);
		list_init(&kernel_cmdlines[i]);
	}
}
