/**
 * @file daemon/opd_sample_files.c
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
extern u32 ctr_count[OP_MAX_COUNTERS];
extern u8 ctr_event[OP_MAX_COUNTERS];
extern u16 ctr_um[OP_MAX_COUNTERS];
extern double cpu_speed;
extern op_cpu cpu_type;

char * opd_mangle_filename(struct opd_image const * image, int counter)
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


/**
 * opd_handle_old_sample_file - deal with old sample file
 * @param mangled  the sample file name
 * @param mtime  the new mtime of the binary
 *
 * If an old sample file exists, verify it is usable.
 * If not, move or delete it. Note than at startup the daemon
 * check than the last (session) events settings match the
 * currents
 */
static void opd_handle_old_sample_file(char const * mangled, time_t mtime)
{
	struct opd_header oldheader;
	FILE * fp;

	fp = fopen(mangled, "r");
	if (!fp) {
		/* file might not be there, or it just might not be
		 * openable for some reason, so try to remove if it exist
		 */
		if (errno == ENOENT)
			goto out;
		else
			goto del;
	}

	if (fread(&oldheader, sizeof(struct opd_header), 1, fp) != 1) {
		verbprintf("Can't read %s\n", mangled);
		goto closedel;
	}

	if (memcmp(&oldheader.magic, OPD_MAGIC, sizeof(oldheader.magic)) || oldheader.version != OPD_VERSION) {
		verbprintf("Magic id check fail for %s\n", mangled);
		goto closedel;
	}

	if (difftime(mtime, oldheader.mtime)) {
		verbprintf("mtime differs for %s\n", mangled);
		goto closedel;
	}

	fclose(fp);
	verbprintf("Re-using old sample file \"%s\".\n", mangled);
	return;

closedel:
	fclose(fp);
del:
	verbprintf("Deleting old sample file \"%s\".\n", mangled);
	remove(mangled);
out:
	;
}


/**
 * opd_handle_old_sample_files - deal with old sample files
 * @param image  the image to check files for
 *
 * to simplify admin of sample file we rename or remove sample
 * files for each counter.
 *
 * If an old sample file exists, verify it is usable.
 * If not, delete it.
 */
void opd_handle_old_sample_files(struct opd_image const * image)
{
	uint i;

	for (i = 0 ; i < op_nr_counters ; ++i) {
		if (ctr_event[i]) {
			char * mangled = opd_mangle_filename(image, i);
			opd_handle_old_sample_file(mangled,  image->mtime);
			free(mangled);
		}
	}
}


/*
 * opd_open_sample_file - open an image sample file
 * @param image  image to open file for
 * @param counter  counter number
 *
 * Open image sample file for the image, counter
 * counter and set up memory mappings for it.
 * image->kernel and image->name must have meaningful
 * values.
 *
 * Returns 0 on success.
 */
int opd_open_sample_file(struct opd_image * image, int counter)
{
	char * mangled;
	samples_odb_t * sample_file;
	struct opd_header * header;
	int err;

	sample_file = &image->sample_files[counter];

	mangled = opd_mangle_filename(image, counter);

	verbprintf("Opening \"%s\"\n", mangled);

	create_path(mangled);

	err = odb_open(sample_file, mangled, ODB_RDWR, sizeof(struct opd_header));

	/* This can naturally happen when racing against opcontrol --reset. */
	if (err != EXIT_SUCCESS) {
		fprintf(stderr, "%s", sample_file->err_msg);
		odb_clear_error(sample_file);
		goto out;
	}

	if (!sample_file->base_memory) {
		err = errno;
		fprintf(stderr,
			"oprofiled: odb_open() of image sample file \"%s\" failed: %s\n",
			mangled, strerror(errno));
		goto out;
	}

	header = sample_file->base_memory;

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

out:
	free(mangled);
	return err;
}


/**
 * @param image  the image pointer to work on
 *
 * sync all samples files belonging to this image
 */
void opd_sync_image_samples_files(struct opd_image * image)
{
	uint i;
	for (i = 0 ; i < op_nr_counters ; ++i) {
		odb_sync(&image->sample_files[i]);
	}
}


/**
 * @param image  the image pointer to work on
 *
 * close all samples files belonging to this image
 */
void opd_close_image_samples_files(struct opd_image * image)
{
	uint i;
	for (i = 0 ; i < op_nr_counters ; ++i) {
		odb_close(&image->sample_files[i]);
	}
}
