/**
 * @file daemon/opd_trans.c
 * Processing the sample buffer
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 */

#include "opd_trans.h"
#include "opd_kernel.h"
#include "opd_sfile.h"
#include "opd_stats.h"
#include "opd_printf.h"
#include "opd_interface.h"
 
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>

extern size_t kernel_pointer_size;

static inline int is_escape_code(uint64_t code)
{
	return kernel_pointer_size == 4 ? code == ~0LU : code == ~0LLU;
}


static uint64_t pop_buffer_value(struct transient * trans)
{
	uint64_t val;

	if (!trans->remaining) {
		fprintf(stderr, "BUG: popping empty buffer !\n");
		abort();
	}

	if (kernel_pointer_size == 4) {
		uint32_t const * lbuf = (void const *)trans->buffer;
		val = *lbuf;
	} else {
		uint64_t const * lbuf = (void const *)trans->buffer;
		val = *lbuf;
	}

	trans->remaining--;
	trans->buffer += kernel_pointer_size;
	return val;
}


static int enough_remaining(struct transient * trans, size_t size)
{
	if (trans->remaining >= size)
		return 1;

	opd_stats[OPD_DANGLING_CODE]++;
	return 0;
}


static void opd_put_sample(struct transient * trans, unsigned long long pc)
{
	unsigned long long event;

	if (!enough_remaining(trans, 1)) {
		trans->remaining = 0;
		return;
	}

	event = pop_buffer_value(trans);

	trans->pc = pc;

	/* sfile can change at each sample for kernel */
	if (trans->in_kernel != 0)
		trans->current = NULL;

	/* get the current sfile if needed */
	if (!trans->current)
		trans->current = sfile_find(trans);

	/* can happen if kernel sample falls through the cracks */
	if (!trans->current)
		return;

	sfile_log_sample(trans->current, trans->pc, event);
}


static void code_unknown(struct transient * trans __attribute__((unused)))
{
	fprintf(stderr, "Unknown code !\n");
	abort();
}


static void code_ctx_switch(struct transient * trans)
{
	trans->current = NULL;

	if (!enough_remaining(trans, 5)) {
		trans->remaining = 0;
		return;
	}

	trans->tid = pop_buffer_value(trans);
	trans->app_cookie = pop_buffer_value(trans);
	/* must be ESCAPE_CODE, CTX_TGID_CODE, tgid. Like this
	 * because tgid was added later in a compatible manner.
	 */
	pop_buffer_value(trans);
	pop_buffer_value(trans);
	trans->tgid = pop_buffer_value(trans);

	if (verbose) {
		char const * app = find_cookie(trans->app_cookie);
		printf("CTX_SWITCH to tid %lu, tgid %lu, cookie %llx(%s)\n",
		       (unsigned long)trans->tid, (unsigned long)trans->tgid,
		       trans->app_cookie, app ? app : "none");
	}
}


static void code_cpu_switch(struct transient * trans)
{
	trans->current = NULL;

	if (!enough_remaining(trans, 1)) {
		trans->remaining = 0;
		return;
	}

	trans->cpu = pop_buffer_value(trans);
	verbprintf("CPU_SWITCH to %lu\n", trans->cpu);
}


static void code_cookie_switch(struct transient * trans)
{
	trans->current = NULL;

	if (!enough_remaining(trans, 1)) {
		trans->remaining = 0;
		return;
	}

	trans->cookie = pop_buffer_value(trans);

	if (verbose) {
		char const * name = verbose_cookie(trans->cookie);
		verbprintf("COOKIE_SWITCH to cookie %s(%llx)\n",
		           name, trans->cookie);
	}
}


static void code_kernel_enter(struct transient * trans)
{
	verbprintf("KERNEL_ENTER_SWITCH to kernel\n");
	trans->in_kernel = 1;
	trans->current = NULL;
	/* subtlety: we must keep trans->cookie cached,
	 * even though it's meaningless for the kernel -
	 * we won't necessarily get a cookie switch on
	 * kernel exit. See comments in opd_sfile.c
	 */
}


static void code_kernel_exit(struct transient * trans)
{
	verbprintf("KERNEL_EXIT_SWITCH to user-space\n");
	trans->in_kernel = 0;
	trans->current = NULL;
}


static void code_module_loaded(struct transient * trans __attribute__((unused)))
{
	verbprintf("MODULE_LOADED_CODE\n");
	opd_reread_module_info();
	trans->current = NULL;
}


typedef void (*handler_t)(struct transient *);

static handler_t handlers[LAST_CODE + 1] = {
	&code_unknown,
	&code_ctx_switch,
	&code_cpu_switch,
	&code_cookie_switch,
	&code_kernel_enter,
	&code_kernel_exit,
	&code_module_loaded,
	&code_unknown /* tgid handled differently */
};


void opd_process_samples(char const * buffer, size_t count)
{
	struct transient trans = {
		.buffer = buffer,
		.remaining = count,
		.current = NULL,
		.cookie = 0,
		.app_cookie = 0,
		.pc = 0,
		.in_kernel = -1,
		.cpu = -1,
		.tid = -1,
		.tgid = -1
	};

	/* FIXME: was uint64_t but it can't compile on alpha where uint64_t
	 * is an unsigned long and below the printf("..." %llu\n", code)
	 * generate a warning, this look like a stopper to use c98 types :/
	 */
	unsigned long long code;

	while (trans.remaining) {
		code = pop_buffer_value(&trans);

		if (!is_escape_code(code)) {
			opd_put_sample(&trans, code);
			continue;
		}

		if (!trans.remaining) {
			verbprintf("Dangling ESCAPE_CODE.\n");
			opd_stats[OPD_DANGLING_CODE]++;
			break;
		}

		// started with ESCAPE_CODE, next is type
		code = pop_buffer_value(&trans);
	
		if (code >= LAST_CODE) {
			fprintf(stderr, "Unknown code %llu\n", code);
			abort();
		}

		handlers[code](&trans);
	}
}
