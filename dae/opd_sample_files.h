/**
 * @file opd_sample_files.h
 * Management of sample files
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon <moz@compsoc.man.ac.uk>
 * @author Philippe Elie <phil_el@wanadoo.fr>
 */

#ifndef OPD_SAMPLE_FILES_H
#define OPD_SAMPLE_FILES_H

struct opd_image;

void opd_sync_image_samples_files(struct opd_image *);
void opd_close_image_samples_files(struct opd_image * image);

void opd_handle_old_sample_files(struct opd_image const * image);
void opd_open_sample_file(struct opd_image * image, int counter);

#endif /* OPD_SAMPLE_FILES_H */
