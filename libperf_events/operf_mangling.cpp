/*
 * @file pe_profiling/operf_mangling.cpp
 * This file is based on daemon/opd_mangling and is used for
 * mangling and opening of sample files for operf.
 *
 * @remark Copyright 2011 OProfile authors
 * @remark Read the file COPYING
 *
 * Created on: Dec 15, 2011
 * @author Maynard Johnson
 * (C) Copyright IBM Corp. 2011
 */

#include <sys/types.h>
#include <iostream>

#include "operf_utils.h"
#include "operf_mangling.h"
#include "operf_kernel.h"
#include "operf_sfile.h"
#include "operf_counter.h"
#include "op_file.h"
#include "op_sample_file.h"
#include "op_mangle.h"
#include "op_events.h"
#include "op_libiberty.h"
#include "cverb.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

extern operf_read operfRead;
extern op_cpu cpu_type;
extern double cpu_speed;

using namespace std;

/* TODO: handle anon
static char * mangle_anon(struct anon_mapping const * anon)
{
	char * name = xmalloc(PATH_MAX);

	snprintf(name, 1024, "%u.0x%llx.0x%llx", (unsigned int)anon->tgid,
	       anon->start, anon->end);

	return name;
}
*/

static char *
mangle_filename(struct operf_sfile * last, struct operf_sfile const * sf, int counter, int cg)
{
	char * mangled;
	struct mangle_values values;
	const struct operf_event * event = operfRead.get_event_by_counter(counter);

	values.flags = 0;

	if (sf->kernel) {
		values.image_name = sf->kernel->name;
		values.flags |= MANGLE_KERNEL;
	} /* TODO: handle anon
	else if (sf->anon) {
		values.flags |= MANGLE_ANON;
		values.image_name = mangle_anon(sf->anon);
		values.anon_name = sf->anon->name;
	}*/
	else {
		values.image_name = sf->image_name;
	}
	values.dep_name = sf->app_filename;
	values.flags |= MANGLE_TGID | MANGLE_TID;
	values.tid = sf->tid;
	values.tgid = sf->tgid;

	if (operf_options::separate_cpu) {
		values.flags |= MANGLE_CPU;
		values.cpu = sf->cpu;
	}

	if (cg) {
		values.flags |= MANGLE_CALLGRAPH;
		if (last->kernel) {
			values.cg_image_name = last->kernel->name;
		}
		/* TODO: handle anon
		 *
		 else if (last->anon) {
			values.flags |= MANGLE_CG_ANON;
			values.cg_image_name = mangle_anon(last->anon);
			values.anon_name = last->anon->name;
		}
		*/
		else {
			values.cg_image_name = last->image_name;
		}
	}

	values.event_name = event->name;
	values.count = event->count;
	values.unit_mask = event->evt_um;

	mangled = op_mangle_filename(&values);

	if (values.flags & MANGLE_ANON)
		free((char *)values.image_name);
	if (values.flags & MANGLE_CG_ANON)
		free((char *)values.cg_image_name);
	return mangled;
}

static void fill_header(struct opd_header * header, unsigned long counter,
                        vma_t anon_start, vma_t cg_to_anon_start,
                        int is_kernel, int cg_to_is_kernel,
                        int spu_samples, uint64_t embed_offset, time_t mtime)
{
	const operf_event_t * event = operfRead.get_event_by_counter(counter);

	memset(header, '\0', sizeof(struct opd_header));
	header->version = OPD_VERSION;
	memcpy(header->magic, OPD_MAGIC, sizeof(header->magic));
	header->cpu_type = cpu_type;
	header->ctr_event = event->op_evt_code;
	header->ctr_count = event->count;
	header->ctr_um = event->evt_um;
	header->is_kernel = is_kernel;
	header->cg_to_is_kernel = cg_to_is_kernel;
	header->cpu_speed = cpu_speed;
	header->mtime = mtime;
	header->anon_start = anon_start;
	header->spu_profile = spu_samples;
	header->embedded_offset = embed_offset;
	header->cg_to_anon_start = cg_to_anon_start;
}

int operf_open_sample_file(odb_t *file, struct operf_sfile *last,
                         struct operf_sfile * sf, int counter, int cg)
{
	char * mangled;
	char const * binary;
	vma_t last_start = 0;
	int err;

	mangled = mangle_filename(last, sf, counter, cg);

	if (!mangled)
		return EINVAL;

	cverb << vsfile << "Opening \"" << mangled << "\"" << endl;

	create_path(mangled);

	/* locking sf will lock associated cg files too */
	operf_sfile_get(sf);
	if (sf != last)
		operf_sfile_get(last);

retry:
	err = odb_open(file, mangled, ODB_RDWR, sizeof(struct opd_header));

	/* This should never happen unless someone is clearing out sample data dir. */
	if (err) {
		if (err == EMFILE) {
			if (operf_sfile_lru_clear()) {
				cerr << "LRU cleared but odb_open() fails for " << mangled << endl;
				abort();
			}
			goto retry;
		}

		cerr << "operf: open of " << mangled << " failed: " << strerror(err) << endl;
		goto out;
	}

	if (!sf->kernel)
		binary = sf->image_name;
	else
		binary = sf->kernel->name;

	/* TODO: handle anon
	if (last && last->anon)
		last_start = last->anon->start;
	 */

	fill_header((struct opd_header *)odb_get_data(file), counter,
		    0, last_start,
		    !!sf->kernel, last ? !!last->kernel : 0,
		    0, 0,
		    binary ? op_get_mtime(binary) : 0);

out:
	operf_sfile_put(sf);
	if (sf != last)
		operf_sfile_put(last);
	free(mangled);
	return err;
}
