/**
 * @file dae/opd_sample_files.c
 * Management of sample files
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 */

#include <sys/types.h>
 
#include "opd_sample_files.h"
#include "opd_image.h"
#include "opd_printf.h"
#include "op_file.h"

#include "op_sample_file.h"
#include "op_config.h"
#include "op_cpu_type.h"
#include "op_mangle.h"
#include "op_events.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

extern uint op_nr_counters;
extern int separate_lib;
extern int separate_kernel;
extern int separate_thread;
extern int separate_cpu;
extern u32 ctr_count[OP_MAX_COUNTERS];
extern u8 ctr_event[OP_MAX_COUNTERS];
extern u16 ctr_um[OP_MAX_COUNTERS];
extern double cpu_speed;
extern op_cpu cpu_type;

/** All sfiles are on this list. */
static LIST_HEAD(lru_list);

/* this value probably doesn't matter too much */
#define LRU_AMOUNT 1000
static int opd_sfile_lru_clear(void)
{
	struct list_head * pos;
	struct list_head * pos2;
	struct opd_sfile * sfile;
	int amount = LRU_AMOUNT;

	verbprintf("image lru clear\n");

	if (list_empty(&lru_list))
		return 1;

	list_for_each_safe(pos, pos2, &lru_list) {
		if (!--amount)
			break;
		sfile = list_entry(pos, struct opd_sfile, lru_next);
		odb_close(&sfile->sample_file);
		list_del_init(&sfile->lru_next);
	}

	return 0;
}


void opd_sfile_lru(struct opd_sfile * sfile)
{
	list_del(&sfile->lru_next);
	list_add_tail(&sfile->lru_next, &lru_list);
}


static char * opd_mangle_filename(struct opd_image const * image, int counter,
                                  int cpu_nr)
{
	char * mangled;
	char const * dep_name = separate_lib ? image->app_name : NULL;
	struct op_event * event = NULL;
	struct mangle_values values;

	if (cpu_type != CPU_TIMER_INT) {
		event = op_find_event(cpu_type, ctr_event[counter]); 
		if (!event) {
			fprintf(stderr, "Unknown event %u for counter %u\n",
				ctr_event[counter], counter);
			abort();
		}
	}

	/* Here we can add TGID, TID, CPU, later.  */
	values.flags = 0;
	if (image->kernel)
		values.flags |= MANGLE_KERNEL;
	if (dep_name && strcmp(dep_name, image->name))
		values.flags |= MANGLE_DEP_NAME;

	if (separate_thread) {
		values.flags |= MANGLE_TGID | MANGLE_TID;
		values.tid = image->tid;
		values.tgid = image->tgid;
	}

	if (separate_cpu) {
		values.flags |= MANGLE_CPU;
		values.cpu = cpu_nr;
	}

	if (cpu_type != CPU_TIMER_INT)
		values.event_name = event->name;
	else
		values.event_name = "TIMER";

	values.count = ctr_count[counter];
	values.unit_mask = ctr_um[counter];

	values.image_name = image->name;
	values.dep_name = dep_name;

	mangled = op_mangle_filename(&values);

	return mangled;
}


/*
 * opd_open_sample_file - open an image sample file
 * @param image  image to open file for
 * @param counter  counter number
 * @param cpu_nr  cpu number
 *
 * Open image sample file for the image, counter
 * counter and set up memory mappings for it.
 * image->kernel and image->name must have meaningful
 * values.
 *
 * Returns 0 on success.
 */
int opd_open_sample_file(struct opd_image * image, int counter, int cpu_nr)
{
	char * mangled;
	struct opd_sfile * sfile;
	struct opd_header * header;
	int err;

	mangled = opd_mangle_filename(image, counter, cpu_nr);

	verbprintf("Opening \"%s\"\n", mangled);

	create_path(mangled);

	sfile = image->sfiles[counter][cpu_nr];
	if (!sfile) {
		sfile = malloc(sizeof(struct opd_sfile));
		list_init(&sfile->lru_next);
		odb_init(&sfile->sample_file);
		image->sfiles[counter][cpu_nr] = sfile;
	}

	list_del(&sfile->lru_next);
	list_add_tail(&sfile->lru_next, &lru_list);

retry:
	err = odb_open(&sfile->sample_file, mangled, ODB_RDWR,
                       sizeof(struct opd_header));

	/* This can naturally happen when racing against opcontrol --reset. */
	if (err) {
		if (err == EMFILE) {
			if (opd_sfile_lru_clear()) {
				printf("LRU cleared but odb_open() fails for %s.\n", mangled);
				abort();
			}
			goto retry;
		}

		fprintf(stderr, "oprofiled: open of %s failed: %s\n",
		        mangled, strerror(err));
		goto out;
	}

	header = sfile->sample_file.base_memory;

	memset(header, '\0', sizeof(struct opd_header));
	header->version = OPD_VERSION;
	memcpy(header->magic, OPD_MAGIC, sizeof(header->magic));
	header->is_kernel = image->kernel;
	header->ctr_event = ctr_event[counter];
	header->ctr_um = ctr_um[counter];
	header->ctr = counter;
	header->cpu_type = cpu_type;
	header->ctr_count = ctr_count[counter];
	header->cpu_speed = cpu_speed;
	header->mtime = image->mtime;
	header->separate_lib = separate_lib;
	header->separate_kernel = separate_kernel;
	/* FIXME separate_thread, separate_cpu */

out:
	free(mangled);
	return err;
}


/**
 * sync all samples files
 */
void opd_sync_samples_files(void)
{
	struct list_head * pos;
	struct opd_sfile * sfile;

	list_for_each(pos, &lru_list) {
		sfile = list_entry(pos, struct opd_sfile, lru_next);
		odb_sync(&sfile->sample_file);
	}
}


/**
 * @param image  the image pointer to work on
 *
 * close all samples files belonging to this image
 */
void opd_close_image_samples_files(struct opd_image * image)
{
	uint i, j;
	for (i = 0 ; i < op_nr_counters ; ++i) {
		for (j = 0; j < NR_CPUS; ++j) {
			if (image->sfiles[i][j]) {
				odb_close(&image->sfiles[i][j]->sample_file);
				list_del(&image->sfiles[i][j]->lru_next);
				free(image->sfiles[i][j]);
				image->sfiles[i][j] = 0;
			}
		}
	}
}
