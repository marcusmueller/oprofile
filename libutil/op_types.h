/**
 * @file op_types.h
 * General-utility types
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 */

#ifndef OP_TYPES_H
#define OP_TYPES_H

#ifndef __KERNEL__
/*@{\name miscellaneous types */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef int fd_t;
/*@}*/
#else
#include <linux/types.h>
#endif

#endif /* OP_TYPES_H */
