/**
 * @file opd_proc.h
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 * 
 * @author John Levon <moz@compsoc.man.ac.uk>
 * @author Philippe Elie <phil_el@wanadoo.fr>
 */

#ifndef OPD_PROC_H
#define OPD_PROC_H
 
#include "oprofiled.h"

extern u32 ctr_count[OP_MAX_COUNTERS];
extern u8 ctr_event[OP_MAX_COUNTERS];
extern u8 ctr_um[OP_MAX_COUNTERS];
extern double cpu_speed;
extern struct op_hash_index *hashmap;
 
extern struct opd_image * kernel_image;

struct opd_image * opd_get_image(const char *name, int hash, const char * app_name, int kernel);
int bstreq(const char *str1, const char *str2);
void opd_put_image_sample(struct opd_image *image, u32 offset, u16 count);
void opd_handle_kernel_sample(u32 eip, u16 count);
void opd_reopen_sample_files(void);

#endif /* OPD_PROC_H */
