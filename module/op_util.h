/**
 * @file op_util.h
 * Various utility functions
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon <moz@compsoc.man.ac.uk>
 * @author Philippe Elie <phil_el@wanadoo.fr>
 */

#ifndef OP_UTIL_H
#define OP_UTIL_H
 
unsigned long kvirt_to_pa(unsigned long adr);
void * rvmalloc(signed long size);
void rvfree(void * mem, signed long size);
// returns non-zero on failure
int check_range(int val, int l, int h, char const * msg);

#endif /* OP_UTIL_H */
