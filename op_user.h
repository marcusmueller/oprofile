/* $Id: op_user.h,v 1.1 2001/01/21 01:11:55 moz Exp $ */
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

#ifndef OP_USER_H
#define OP_USER_H

/* stuff shared between user-space and the module */

#include "version.h"

#ifndef NR_CPUS 
#define NR_CPUS 32
#endif 

/* change these, you change them in op_start as well,
 * you hear ?
 */
/* 262144 * 8 = 2097152 bytes default */
#define OP_DEFAULT_BUF_SIZE 262144
/* 32768 * 32 = 1048756 bytes default */
#define OP_DEFAULT_HASH_SIZE 32768

#define OP_BITS 2

/* top OP_BITS bits of count are used as follows: */
/* is this actually a notification ? */
#define OP_NOTE (1U<<15)
/* which perf counter the sample is from */
#define OP_COUNTER (1U<<14)

#define OP_COUNT_MASK ((1U<<(16-OP_BITS))-1U)

/* mapping notification types */
/* fork(),vfork(),clone() */
#define OP_FORK (OP_NOTE|(1U<<0))
/* mapping */
#define OP_MAP (OP_NOTE|(1U<<14))
/* execve() */
#define OP_EXEC (OP_NOTE|(1U<<14)|(1U<<13))
/* init_module() */
#define OP_DROP_MODULES (OP_NOTE|(1U<<1))
/* exit() */
#define OP_EXIT (OP_NOTE|(1U<<2))

#define IS_OP_MAP(v) ( \
	((v) & OP_NOTE) && \
	((v) & (1U<<14)) )

#define IS_OP_EXEC(v) ( \
	((v) & OP_NOTE) && \
	((v) & (1U<<14)) && \
	((v) & (1U<<13)) )

/* note that pid_t is 32 bits, but only 16 are used
   currently, so to save cache, we use u16 */
struct op_sample {
	u16 count;
	u16 pid;
	u32 eip;
} __attribute__((__packed__,__aligned__(8)));

#ifndef __ok_unused
#define __ok_unused __attribute((__unused))
#endif

/* nr. entries in hash map, prime
 * this is the maximum number of name components allowed
 * This is the maximal value we have bits for
 */
#define OP_HASH_MAP_NR 4093

/* size of hash map entries */
#define OP_HASH_LINE 128

struct op_hash {
	char name[OP_HASH_LINE];
	u16 parent;
} __attribute__((__packed__));

/* temporary mapping structure */
struct op_mapping {
	u16 pid;
	u32 addr;
	u32 len;
	u32 offset;
	short hash;
	int is_execve; 
};

/* size of hash map in bytes */
#define OP_HASH_MAP_SIZE (OP_HASH_MAP_NR*sizeof(struct op_hash))

#endif /* OP_USER_H */
