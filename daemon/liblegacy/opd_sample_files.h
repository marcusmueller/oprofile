/**
 * @file dae/opd_sample_files.h
 * Management of sample files
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 */

#ifndef OPD_SAMPLE_FILES_H
#define OPD_SAMPLE_FILES_H

#include "op_list.h"
#include "odb_hash.h"

struct opd_image;

struct opd_24_sfile {
	struct list_head lru_next;
	samples_odb_t sample_file;
};

void opd_sync_samples_files(void);
void opd_close_image_samples_files(struct opd_image * image);
int opd_open_24_sample_file(struct opd_image * image, int counter, int cpu_nr);

void opd_24_sfile_lru(struct opd_24_sfile * sfile);


#endif /* OPD_SAMPLE_FILES_H */
