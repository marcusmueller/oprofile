/**
 * @file opd_mapping.h
 * Management of process mappings
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 * 
 * @author John Levon <moz@compsoc.man.ac.uk>
 * @author Philippe Elie <phil_el@wanadoo.fr>
 */

#ifndef OPD_MAPPING_H
#define OPD_MAPPING_H
 
#include "op_types.h"
 
struct opd_image;
struct opd_proc;
struct op_note;
 
struct opd_map {
	struct opd_image * image;
	u32 start;
	u32 offset;
	u32 end;
};

void opd_init_hash_map(void);
void opd_init_maps(struct opd_proc * proc);
void opd_handle_mapping(struct op_note const * note);
void opd_grow_maps(struct opd_proc * proc);
void opd_kill_maps(struct opd_proc * proc);

/**
 * opd_is_in_map - check whether an EIP is within a mapping
 * @param map  map to check
 * @param eip  EIP value
 *
 * Return %1 if the EIP value @eip is within the boundaries
 * of the map @map, %0 otherwise.
 */
inline static int opd_is_in_map(struct opd_map * map, u32 eip)
{
	return (eip >= map->start && eip < map->end);
}

 
/* 
 * opd_map_offset - return offset of sample against map
 * @param map  map to use
 * @param eip  EIP value to use
 *
 * Returns the offset of the EIP value @eip into
 * the map @map, which is the same as the file offset
 * for the relevant binary image.
 */
inline static u32 opd_map_offset(struct opd_map * map, u32 eip)
{
	return (eip - map->start) + map->offset;
}

#endif /* OPD_MAPPING */
