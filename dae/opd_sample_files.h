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

#include "opd_image.h"

#ifndef OPD_SAMPLE_FILES_H
#define OPD_SAMPLE_FILES_H
 
void opd_handle_old_sample_files(struct opd_image const * image);
void opd_open_sample_file(struct opd_image * image, int counter);

#endif /* OPD_SAMPLE_FILES_H */
