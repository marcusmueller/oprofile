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
