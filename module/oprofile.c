/* COPYRIGHT (C) 2000 THE VICTORIA UNIVERSITY OF MANCHESTER and John Levon
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "oprofile.h"

EXPORT_NO_SYMBOLS;

static char *op_version = VERSION_STRING;
MODULE_AUTHOR("John Levon (moz@compsoc.man.ac.uk)");
MODULE_DESCRIPTION("Continuous Profiling Module");
MODULE_LICENSE("GPL");

MODULE_PARM(allow_unload, "i");
MODULE_PARM_DESC(allow_unload, "Allow module to be unloaded.");
#ifdef CONFIG_SMP
static int allow_unload;
#else
static int allow_unload = 1;
#endif

/* sysctl settables */
struct oprof_sysctl sysctl_parms;
/* some of the sys ctl settable variable needs to be copied to protect
 * against user that try to change through /proc/sys/dev/oprofile/ * running
 * parameters during profiling */
struct oprof_sysctl sysctl;

static u32 prof_on __cacheline_aligned_in_smp;

/* in the process of quitting ? */
static int quitting;
/* is partial_stop made ?  Re-using quitting for this purpose is obfuscated */
int partial_stop;

static int op_major;

static volatile uint oprof_opened __cacheline_aligned_in_smp;
static volatile uint oprof_note_opened __cacheline_aligned_in_smp;
static DECLARE_WAIT_QUEUE_HEAD(oprof_wait);

static u32 oprof_ready[NR_CPUS] __cacheline_aligned_in_smp;
struct _oprof_data oprof_data[NR_CPUS];

struct op_note * note_buffer __cacheline_aligned_in_smp;
u32 note_pos __cacheline_aligned_in_smp;

// the interrupt handler ops structure to use
static struct op_int_operations * int_ops;

/* ---------------- interrupt entry routines ------------------ */

inline static int need_wakeup(uint cpu, struct _oprof_data * data)
{
	return data->nextbuf >= (data->buf_size - OP_PRE_WATERMARK) && !oprof_ready[cpu];
}

inline static void next_sample(struct _oprof_data * data)
{
	if (unlikely(++data->nextbuf == data->buf_size))
		data->nextbuf = 0;
}

inline static void evict_op_entry(uint cpu, struct _oprof_data * data, const struct op_sample *ops, const struct pt_regs *regs)
{
	memcpy(&data->buffer[data->nextbuf], ops, sizeof(struct op_sample));
	next_sample(data);
	if (likely(!need_wakeup(cpu, data)))
		return;

	/* locking rationale :
	 *
	 * other CPUs are not a race concern since we synch on oprof_wait->lock.
	 *
	 * for the current CPU, we might have interrupted another user of e.g.
	 * runqueue_lock, deadlocking on SMP and racing on UP. So we check that IRQs
	 * were not disabled (corresponding to the irqsave/restores in __wake_up().
	 *
	 * Note that this requires all spinlocks taken by the full wake_up path
	 * to have saved IRQs - otherwise we can interrupt whilst holding a spinlock
	 * taken from some non-wake_up() path and deadlock. Currently this means only
	 * oprof_wait->lock and runqueue_lock: all instances disable IRQs before
	 * taking the lock.
	 *
	 * This will mean that approaching the end of the buffer, a number of the
	 * evictions may fail to wake up the daemon. We simply hope this doesn't
	 * take long; a pathological case could cause buffer overflow (which will
	 * be less of an issue when we have a separate map device anyway).
	 *
	 * Note that we use oprof_ready as our flag for whether we have initiated a
	 * wake-up. Once the wake-up is received, the flag is reset as well as
	 * data->nextbuf, preventing multiple wakeups.
	 *
	 * On 2.2, a global waitqueue_lock is used, so we must check it's not held
	 * by the current CPU. We make sure that any users of the wait queue (i.e.
	 * us and the code for wait_event_interruptible()) disable interrupts so it's
	 * still safe to check IF_MASK.
	 */
	if (likely(regs->eflags & IF_MASK)) {
		oprof_ready[cpu] = 1;
		wake_up(&oprof_wait);
	}
}

inline static void fill_op_entry(struct op_sample *ops, struct pt_regs *regs, int ctr)
{
	ops->eip = regs->eip;
	ops->pid = current->pid;
	ops->count = (1U << OP_BITS_COUNT)*ctr + 1;
}

void regparm3 op_do_profile(uint cpu, struct pt_regs *regs, int ctr)
{
	struct _oprof_data * data = &oprof_data[cpu];
	uint h, i; 
	struct op_sample * samples;

	data->nr_irq++;

	h = op_hash(regs->eip, current->pid, ctr);
	samples = data->entries[h].samples;

	for (i=0; i < OP_NR_ENTRY; i++) {
		if (likely(!op_miss(samples[i]))) {
			samples[i].count++;
			return;
		}
	}
 
	evict_op_entry(cpu, data, &samples[data->next], regs);
	fill_op_entry(&samples[data->next], regs, ctr);
	data->next = (data->next + 1) % OP_NR_ENTRY;
	return;
}

/* ---------------- driver routines ------------------ */

spinlock_t note_lock __cacheline_aligned_in_smp = SPIN_LOCK_UNLOCKED;
uint cpu_num;

static int is_ready(void)
{
	for (cpu_num=0; cpu_num < smp_num_cpus; cpu_num++) {
		if (oprof_ready[cpu_num])
			return 1;
	}
	return 0;
}

inline static void up_and_check_note(void)
{
	note_pos++;
	if (likely(note_pos < (sysctl.note_size - OP_PRE_NOTE_WATERMARK) && !is_ready()))
		return;

	/* if we reach the end of the buffer, just pin
	 * to the last entry until it is read. This loses
	 * notes, but we have no choice. */
	if (unlikely(note_pos == sysctl.note_size)) {
		static int warned;
		if (!warned) {
			printk(KERN_WARNING "note buffer overflow: restart "
			       "oprofile with a larger note buffer.\n");
			warned = 1;
		}
		note_pos = sysctl.note_size - 1;
	}

	/* we just use cpu 0 as a convenient one to wake up */
	oprof_ready[0] = 2;
	oprof_wake_up(&oprof_wait);
}

// if holding note_lock
void __oprof_put_note(struct op_note *onote)
{
	if (!prof_on)
		return;

	memcpy(&note_buffer[note_pos], onote, sizeof(struct op_note));
	up_and_check_note();
}
 
void oprof_put_note(struct op_note *onote)
{
	spin_lock(&note_lock);
	__oprof_put_note(onote);
	spin_unlock(&note_lock);
}

static int oprof_note_read(char *buf, size_t count, loff_t *ppos)
{
	struct op_note *mybuf;
	uint num;
	ssize_t max;

	max = sizeof(struct op_note) * sysctl.note_size;

	if (*ppos || count != max)
		return -EINVAL;

	mybuf = vmalloc(max);
	if (!mybuf)
		return -EFAULT;

	spin_lock(&note_lock);

	num = note_pos;

	count = note_pos * sizeof(struct op_note);

	if (count)
		memcpy(mybuf, note_buffer, count);

	note_pos = 0;

	spin_unlock(&note_lock);

	if (count && copy_to_user(buf, mybuf, count))
		count = -EFAULT;

	vfree(mybuf);
	return count;
}

static int oprof_note_open(void)
{
	if (test_and_set_bit(0, &oprof_note_opened))
		return -EBUSY;
	INC_USE_COUNT_MAYBE;
	return 0;
}

static int oprof_note_release(void)
{
	if (!oprof_note_opened)
		return -EFAULT;

	clear_bit(0, &oprof_note_opened);
	DEC_USE_COUNT_MAYBE;
	return 0;
}

static int check_buffer_amount(struct _oprof_data * data)
{
	int size = data->buf_size;
	int num = data->nextbuf;
	if (num < size - OP_PRE_WATERMARK && oprof_ready[cpu_num] != 2) {
		printk(KERN_WARNING "oprofile: Detected overflow of size %d. You must increase "
			"the hash table size or reduce the interrupt frequency\n", num);
		num = size;
	} else
		data->nextbuf=0;
	return num;
}

static int oprof_read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
	uint num;
	ssize_t max;

	if (!capable(CAP_SYS_PTRACE))
		return -EPERM;

	if (!prof_on) {
		kill_proc(SIGKILL, current->pid, 1);
		return -EINTR;
	}

	switch (minor(file->f_dentry->d_inode->i_rdev)) {
		case 2: return oprof_note_read(buf, count, ppos);
		case 0: break;
		default: return -EINVAL;
	}

	max = sizeof(struct op_sample) * sysctl.buf_size;

	if (*ppos || count != max)
		return -EINVAL;

	if (file->f_flags & O_NONBLOCK) {
		uint cpu;
		for (cpu = 0; cpu < smp_num_cpus; ++cpu) {
			if (oprof_data[cpu].nextbuf) {
				cpu_num = cpu;
				oprof_ready[cpu_num] = 2;
				break;
			}
		}
		if (cpu == smp_num_cpus)
			return -EAGAIN;
	} else if (quitting) {
		/* we might have done dump_stop just before the daemon
		 * is about to sleep */
		quitting = 0;
		return 0;
	} else {
		wait_event_interruptible(oprof_wait, is_ready());
	}

	/* on SMP, we may have already dealt with the signal between
	 * the wake up from the signal and this point, this point,
	 * so we might go on to copy some data. But that's OK.
	 */
	if (signal_pending(current))
		return -EINTR;

	/* if we are quitting, return 0 read to tell daemon */
	if (quitting) {
		quitting = 0;
		return 0;
	}

	int_ops->stop_cpu(cpu_num);

	/* buffer might have overflowed */
	num = check_buffer_amount(&oprof_data[cpu_num]);

	oprof_ready[cpu_num] = 0;

	count = num * sizeof(struct op_sample);

	if (count && copy_to_user(buf, oprof_data[cpu_num].buffer, count))
		count = -EFAULT;

	int_ops->start_cpu(cpu_num);

	/* 0 is a special case for us, prefer -EINTR instead. Ugly. */
	if (!count)
		return -EINTR;
	return count;
}

static int oprof_start(void);
static int oprof_stop(void);

static int oprof_open(struct inode *ino, struct file *file)
{
	int err;

	if (!capable(CAP_SYS_PTRACE))
		return -EPERM;

	switch (minor(file->f_dentry->d_inode->i_rdev)) {
		case 1: return oprof_hash_map_open();
		case 2: return oprof_note_open();
		case 0:
			/* make sure the other devices are open */
			if (is_map_ready())
				break;
		default:
			return -EINVAL;
	}

	if (test_and_set_bit(0, &oprof_opened))
		return -EBUSY;

	err = oprof_start();
	if (err)
		clear_bit(0, &oprof_opened);
	return err;
}

static int oprof_release(struct inode *ino, struct file *file)
{
	switch (minor(file->f_dentry->d_inode->i_rdev)) {
		case 1: return oprof_hash_map_release();
		case 2: return oprof_note_release();
		case 0: break;
		default: return -EINVAL;
	}

	if (!oprof_opened)
		return -EFAULT;

	/* finished quitting */
	quitting = 0;
	/* the block on re-starting is over */
	partial_stop = 0;

	clear_bit(0, &oprof_opened);

	return oprof_stop();
}

static int oprof_mmap(struct file *file, struct vm_area_struct *vma)
{
	if (minor(file->f_dentry->d_inode->i_rdev) == 1)
		return oprof_hash_map_mmap(file, vma);
	return -EINVAL;
}

/* called under spinlock, cannot sleep */
static void oprof_free_mem(uint num)
{
	uint i;
	for (i=0; i < num; i++) {
		if (oprof_data[i].entries)
			vfree(oprof_data[i].entries);
		if (oprof_data[i].buffer)
			vfree(oprof_data[i].buffer);
		oprof_data[i].entries = NULL;
		oprof_data[i].buffer = NULL;
	}
	vfree(note_buffer);
	note_buffer = NULL;
}

static int oprof_init_data(void)
{
	uint i;
	ulong hash_size,buf_size;
	struct _oprof_data *data;

	note_buffer = vmalloc(sizeof(struct op_note) * sysctl.note_size);
 	if (!note_buffer) {
		printk(KERN_ERR "oprofile: failed to allocate not buffer of %u bytes\n",
			sizeof(struct op_note) * sysctl.note_size);
		return -EFAULT;
	}
	note_pos = 0;

	for (i=0; i < smp_num_cpus; i++) {
		data = &oprof_data[i];
		hash_size = (sizeof(struct op_entry) * sysctl.hash_size);
		buf_size = (sizeof(struct op_sample) * sysctl.buf_size);

		data->entries = vmalloc(hash_size);
		if (!data->entries) {
			printk(KERN_ERR "oprofile: failed to allocate hash table of %lu bytes\n",hash_size);
			oprof_free_mem(i);
			return -EFAULT;
		}

		data->buffer = vmalloc(buf_size);
		if (!data->buffer) {
			printk(KERN_ERR "oprofile: failed to allocate eviction buffer of %lu bytes\n",buf_size);
			vfree(data->entries);
			oprof_free_mem(i);
			return -EFAULT;
		}

		memset(data->entries, 0, hash_size);
		memset(data->buffer, 0, buf_size);

		data->hash_size = sysctl.hash_size;
		data->buf_size = sysctl.buf_size;
	}

	return 0;
}

static int parms_check(void)
{
	int err = 0;
	uint cpu;
	struct _oprof_data *data;

	op_check_range(sysctl.hash_size, 256, 262144,
		"sysctl.hash_size value %d not in range (%d %d)\n");
	op_check_range(sysctl.buf_size, OP_PRE_WATERMARK + 1024, 1048576,
		"sysctl.buf_size value %d not in range (%d %d)\n");
	op_check_range(sysctl.note_size, OP_PRE_NOTE_WATERMARK + 1024, 1048576,
		"sysctl.note_size value %d not in range (%d %d)\n");

	if ((err = int_ops->check_params()))
		return err;

	for (cpu=0; cpu < smp_num_cpus; cpu++) {
		data = &oprof_data[cpu];

		/* make sure the buffer and hash table have been set up */
		if (!data->buffer || !data->entries)
			return -EFAULT;
	}

	return err;
}


DECLARE_MUTEX(sysctlsem);


static int oprof_start(void)
{
	int err = 0;

	down(&sysctlsem);

	/* save the sysctl settable things to protect against change through
	 * systcl the profiler params */
	sysctl_parms.cpu_type = sysctl.cpu_type;
	sysctl = sysctl_parms;

	if ((err = oprof_init_data()))
		goto out;

	if ((err = parms_check())) {
		oprof_free_mem(smp_num_cpus);
		goto out;
	}

	if ((err = int_ops->setup())) {
		oprof_free_mem(smp_num_cpus);
		goto out;
	}

	if (!sysctl.kernel_only)
		op_intercept_syscalls();

	int_ops->start();

	prof_on = 1;

out:
	up(&sysctlsem);
	return err;
}

/*
 * stop interrupts being generated and notes arriving.
 * This needs to be idempotent.
 */
static void oprof_partial_stop(void)
{
	if (partial_stop)
		return;

	op_replace_syscalls();

	int_ops->stop();

	partial_stop = 1;
}

static int oprof_stop(void)
{
	uint i;
	int err = -EINVAL;

	down(&sysctlsem);

	if (!prof_on)
		goto out;

	/* here we need to :
	 * bring back the old system calls
	 * stop the perf counter
	 * bring back the old NMI handler
	 * reset the map buffer stuff and ready values
	 *
	 * Nothing will be able to write into the map buffer because
	 * we check explicitly for prof_on and synchronise via the spinlocks
	 */

	prof_on = 0;

	oprof_partial_stop();

	spin_lock(&note_lock);

	for (i=0; i < smp_num_cpus; i++) {
		struct _oprof_data *data = &oprof_data[i];
		oprof_ready[i] = 0;
		data->nextbuf = data->next = 0;
	}

	oprof_free_mem(smp_num_cpus);

	spin_unlock(&note_lock);
	err = 0;

out:
	up(&sysctlsem);
	return err;
}

static struct file_operations oprof_fops = {
#ifdef HAVE_FILE_OPERATIONS_OWNER
	owner: THIS_MODULE,
#endif
	open: oprof_open,
	release: oprof_release,
	read: oprof_read,
	mmap: oprof_mmap,
};

/*
 * /proc/sys/dev/oprofile/
 *                        bufsize
 *                        hashsize
 *                        notesize
 *                        dump
 *                        dump_stop
 *                        kernel_only
 *                        pid_filter
 *                        pgrp_filter
 *                        nr_interrupts
 *                        #ctr/
 *                          event
 *                          enabled
 *                          count
 *                          unit_mask
 *                          kernel
 *                          user
 *
 * #ctr is in [0-1] for PPro core, [0-3] for Athlon core
 *
 */

/* These access routines are basically not safe on SMP for module unload.
 * And there is nothing we can do about it - the API is broken. We'll just
 * make a best-efforts thing. Note the sem is needed to prevent parms_check
 * bypassing during oprof_start().
 */

static void lock_sysctl(void)
{
	MOD_INC_USE_COUNT;
	down(&sysctlsem);
}

static void unlock_sysctl(void)
{
	up(&sysctlsem);
	MOD_DEC_USE_COUNT;
}

static int get_nr_interrupts(ctl_table *table, int write, struct file *filp, void *buffer, size_t *lenp)
{
	uint cpu;
	int ret = -EINVAL;

	lock_sysctl();

	if (write)
		goto out;

	sysctl.nr_interrupts = 0;

	for (cpu=0; cpu < smp_num_cpus; cpu++) {
		sysctl.nr_interrupts += oprof_data[cpu].nr_irq;
		oprof_data[cpu].nr_irq = 0;
	}

	ret =  proc_dointvec(table, write, filp, buffer, lenp);
out:
	unlock_sysctl();
	return ret;
}

int lproc_dointvec(ctl_table *table, int write, struct file *filp, void *buffer, size_t *lenp)
{
	int err;

	lock_sysctl();
	err = proc_dointvec(table, write, filp, buffer, lenp);
	unlock_sysctl();

	return err;
}

static void dump_one(struct _oprof_data *data, struct op_sample *ops, uint cpu)
{
	if (!ops->count)
		return;

	memcpy(&data->buffer[data->nextbuf], ops, sizeof(struct op_sample));

	ops->count = 0;

	next_sample(data);
	if (likely(!need_wakeup(cpu, data)))
		return;
	oprof_ready[cpu] = 1;
}

static void do_actual_dump(void)
{
	uint cpu;
	int i,j;

	/* clean out the hash table as far as possible */
	for (cpu=0; cpu < smp_num_cpus; cpu++) {
		struct _oprof_data * data = &oprof_data[cpu];
		spin_lock(&note_lock);
		int_ops->stop_cpu(cpu);
		for (i=0; i < data->hash_size; i++) {
			for (j=0; j < OP_NR_ENTRY; j++)
				dump_one(data, &data->entries[i].samples[j], cpu);
			if (oprof_ready[cpu])
				break;
		}
		spin_unlock(&note_lock);
		oprof_ready[cpu] = 2;
		int_ops->start_cpu(cpu);
	}
	oprof_wake_up(&oprof_wait);
}

static int sysctl_do_dump(ctl_table *table, int write, struct file *filp, void *buffer, size_t *lenp)
{
	int err = -EINVAL;

	lock_sysctl();

	if (!prof_on)
		goto out;

	if (!write) {
		err = proc_dointvec(table, write, filp, buffer, lenp);
		goto out;
	}

	do_actual_dump();

	err = 0;
out:
	unlock_sysctl();
	return err;
}

static int sysctl_do_dump_stop(ctl_table *table, int write, struct file *filp, void *buffer, size_t *lenp)
{
	int err = -EINVAL;

	lock_sysctl();

	if (!prof_on)
		goto out;

	if (!write) {
		err = proc_dointvec(table, write, filp, buffer, lenp);
		goto out;
	}

	/* this is unfortunate, but we have to make sure we don't enable
	 * interrupts again, and the daemon knows to quit
	 */
	quitting = 1;

	oprof_partial_stop();

	/* also wakes up daemon */
	do_actual_dump();

	err = 0;
out:
	unlock_sysctl();
	return err;
}

int nr_oprof_static = 10;

static ctl_table oprof_table[] = {
	{ 1, "bufsize", &sysctl_parms.buf_size, sizeof(int), 0644, NULL, &lproc_dointvec, NULL, },
	{ 1, "hashsize", &sysctl_parms.hash_size, sizeof(int), 0644, NULL, &lproc_dointvec, NULL, },
	{ 1, "dump", &sysctl_parms.dump, sizeof(int), 0666, NULL, &sysctl_do_dump, NULL, },
	{ 1, "dump_stop", &sysctl_parms.dump_stop, sizeof(int), 0644, NULL, &sysctl_do_dump_stop, NULL, },
	{ 1, "kernel_only", &sysctl_parms.kernel_only, sizeof(int), 0644, NULL, &lproc_dointvec, NULL, },
	{ 1, "pid_filter", &sysctl_parms.pid_filter, sizeof(pid_t), 0644, NULL, &lproc_dointvec, NULL, },
	{ 1, "pgrp_filter", &sysctl_parms.pgrp_filter, sizeof(pid_t), 0644, NULL, &lproc_dointvec, NULL, },
	{ 1, "nr_interrupts", &sysctl.nr_interrupts, sizeof(int), 0444, NULL, &get_nr_interrupts, NULL, },
	{ 1, "notesize", &sysctl_parms.note_size, sizeof(int), 0644, NULL, &lproc_dointvec, NULL, },
	{ 1, "cpu_type", &sysctl.cpu_type, sizeof(int), 0444, NULL, &lproc_dointvec, NULL, },
	{ 0, }, { 0, }, { 0, }, { 0, },
	{ 0, },
};

static ctl_table oprof_root[] = {
	{1, "oprofile", NULL, 0, 0755, oprof_table},
 	{0,},
};

static ctl_table dev_root[] = {
	{CTL_DEV, "dev", NULL, 0, 0555, oprof_root},
	{0,},
};

static struct ctl_table_header *sysctl_header;

/* NOTE: we do *not* support sysctl() syscall */

static int __init init_sysctl(void)
{
	int err = 0;
	ctl_table *next = &oprof_table[nr_oprof_static];

	/* these sysctl parms need sensible value */
	sysctl_parms.hash_size = OP_DEFAULT_HASH_SIZE;
	sysctl_parms.buf_size = OP_DEFAULT_BUF_SIZE;
	sysctl_parms.note_size = OP_DEFAULT_NOTE_SIZE;

	if ((err = int_ops->add_sysctls(next))) {
		return err;
	}

	sysctl_header = register_sysctl_table(dev_root, 0);
	return err;
}

/* not safe to mark as __exit since used from __init code */
static void cleanup_sysctl(void)
{
	ctl_table *next = &oprof_table[nr_oprof_static];
	unregister_sysctl_table(sysctl_header);
	
	int_ops->remove_sysctls(next);

	return;
}

static int can_unload(void)
{
	int can = -EBUSY;
	down(&sysctlsem);

	if (allow_unload && !prof_on && !GET_USE_COUNT(THIS_MODULE))
		can = 0;
	up(&sysctlsem);
	return can;
}

int __init oprof_init(void)
{
	int err = 0;

	if (sysctl.cpu_type != CPU_RTC) {
		int_ops = &op_nmi_ops;

		// try to init, fall back to rtc if not
		if ((err = int_ops->init())) {
			int_ops = &op_rtc_ops;
			if ((err = int_ops->init()))
				return err;
			sysctl.cpu_type = CPU_RTC;
		}
	} else {
		int_ops = &op_rtc_ops;
		if ((err = int_ops->init()))
			return err;
	}

	if ((err = init_sysctl()))
		goto out_err;

 	err = op_major = register_chrdev(0, "oprof", &oprof_fops);
	if (err < 0)
		goto out_err2;

	err = oprof_init_hashmap();
	if (err < 0) {
		printk(KERN_ERR "oprofile: couldn't allocate hash map !\n");
		unregister_chrdev(op_major, "oprof");
		goto out_err2;
	}

	/* module might not be unloadable */
	THIS_MODULE->can_unload = can_unload;

	/* do this now so we don't have to track save/restores later */
	op_save_syscalls();

	printk(KERN_INFO "%s loaded, major %u\n", op_version, op_major);
	return 0;

out_err2:
	cleanup_sysctl();
out_err:
	int_ops->deinit();
	return err;
}

void __exit oprof_exit(void)
{
	oprof_free_hashmap();

	unregister_chrdev(op_major, "oprof");

	cleanup_sysctl();

	int_ops->deinit();
}

/*
 * "The most valuable commodity I know of is information."
 *      - Gordon Gekko
 */
