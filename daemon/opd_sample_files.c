/**
 * @file opd_sample_files.c
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

#include "op_sample_file.h"
#include "op_interface_25.h"
#include "op_cpu_type.h"
#include "op_mangle.h"

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

extern uint op_nr_counters;
extern int separate_samples;
extern u32 ctr_count[OP_MAX_COUNTERS];
extern u8 ctr_event[OP_MAX_COUNTERS];
extern u8 ctr_um[OP_MAX_COUNTERS];
extern double cpu_speed;
extern op_cpu cpu_type;

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
		 * openable for some reason, so try to remove anyway
		 */
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
	char * mangled;
	uint len;
	char const * app_name = separate_samples ? image->app_name : NULL;

	mangled = op_mangle_filename(image->name, app_name);

	len = strlen(mangled);

	for (i = 0 ; i < op_nr_counters ; ++i) {
		sprintf(mangled + len, "#%d", i);
		opd_handle_old_sample_file(mangled,  image->mtime);
	}

	free(mangled);
}


/*
 * opd_open_sample_file - open an image sample file
 * @param image  image to open file for
 * @param counter  counter number
 *
 * Open image sample file for the image @image, counter
 * @counter and set up memory mappings for it.
 * image->kernel and image->name must have meaningful
 * values.
 */
// FIXME: take db_tree_t * instead
void opd_open_sample_file(struct opd_image * image, int counter)
{
	char * mangled;
	db_tree_t * sample_file;
	struct opd_header * header;
	char const * app_name;

	sample_file = &image->sample_files[counter];

	app_name = separate_samples ? image->app_name : NULL;
	mangled = op_mangle_filename(image->name, app_name);

	sprintf(mangled + strlen(mangled), "#%d", counter);

	verbprintf("Opening \"%s\"\n", mangled);

	db_open(sample_file, mangled, DB_RDWR, sizeof(struct opd_header));
	if (!sample_file->base_memory) {
		fprintf(stderr,
			"oprofiled: db_open() of image sample file \"%s\" failed: %s\n",
			mangled, strerror(errno));
		goto err;
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
	header->separate_samples = separate_samples;

err:
	free(mangled);
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
		db_sync(&image->sample_files[i]);
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
		db_close(&image->sample_files[i]);
	}
}
