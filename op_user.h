/* $Id: op_user.h,v 1.19 2002/03/19 21:15:14 phil_e Exp $ */
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

#ifndef u8
#define u8 unsigned char
#endif
#ifndef u16
#define u16 unsigned short
#endif
#ifndef u32
#define u32 unsigned int
#endif
#ifndef uint
#define uint unsigned int
#endif
#ifndef ulong
#define ulong unsigned long
#endif
#ifndef fd_t
#define fd_t int
#endif


/* FIXME: must we use group command, I show this as an example and
 * do not group other things */
/** \defgroup op_check_events_evt_code op_check_events() return code */
/*@{*/
#define OP_EVENTS_OK		0x0
/** The event number is invalid */
#define OP_EVT_NOT_FOUND	0x1
/** The event have no unit mask */
#define OP_EVT_NO_UM		0x2
/** The event is not available for the selected counter */
#define OP_EVT_CTR_NOT_ALLOWED	0x4
/*@}*/

/** supported cpu type */
typedef enum {
	CPU_NO_GOOD = -1,
	CPU_PPRO,
	CPU_PII,
	CPU_PIII,
	CPU_ATHLON,
	CPU_RTC,
	MAX_CPU_TYPE
} op_cpu;

#ifndef NR_CPUS
#define NR_CPUS 32
#endif 

/* change these, you change them in op_start as well,
 * you hear ?
 */
/** 65536 * 32 = 2097152 bytes default */
#define OP_DEFAULT_HASH_SIZE 65536
/** 16384 * 8 = 131072 bytes default */
#define OP_DEFAULT_BUF_SIZE 16384
/** note buffer size */
#define OP_DEFAULT_NOTE_SIZE 16384

/** kernel image entries are offset by this many entries */
#define OPD_KERNEL_OFFSET 524288

/** maximum nr. of counters, up to 4 for Athlon (18 for P4). The primary use
 * of this variable is for static/local array dimension. Never use it in loop
 * or in array index access/index checking. Don't change it without updating
 * OP_BITS_CTR! */
#define OP_MAX_COUNTERS	4

/** the number of bits neccessary to store OP_MAX_COUNTERS values */
#define OP_BITS	2

/** The number of bits available to store count. The 16 value is
 * sizeof_in_bits(op_sample.count)  */
#define OP_BITS_COUNT	(16 - OP_BITS)

/** counter nr mask */
#define OP_CTR_MASK	((~0U << (OP_BITS_COUNT + 1)) >> 1)

/** top OP_BITS bits of count are used as follows: */
/* which perf counter the sample is from */
#define OP_COUNTER(x)	(((x) & OP_CTR_MASK) >> OP_BITS_COUNT)

#define OP_COUNT_MASK	((1U << OP_BITS_COUNT) - 1U)

/* notifications types encoded in op_note::type */
/** fork(),vfork(),clone() */
#define OP_FORK 1
/** mapping */
#define OP_MAP 2
/** execve() */
#define OP_EXEC 4
/** init_module() */
#define OP_DROP_MODULES 8
/** exit() */
#define OP_EXIT 16

/** Data type to transfer samples counts from the module to the daemon */
struct op_sample {
	/** samples count; high order bits contains the counter nr */
	u16 count;
	u16 pid;	/**< 32 bits but only 16 bits are used currently */
	u32 eip;	/**< eip value where occur interrupt */
} __attribute__((__packed__, __aligned__(8)));

/** Data type used by the module to notify daemon of fork/exit/mapping etc.
 * Meanings of fields depend on the type of notification
 * \sa OP_FORK, OP_EXEC, OP_MAP, OP_DROP_MODULES and OP_EXIT */
struct op_note {
	u32 addr;
	u32 len;
	u32 offset;
	uint hash;
	u16 pid;
	u16 type;
};

/** nr. entries in hash map. This is the maximum number of name components
 * allowed. Must be a prime number */
#define OP_HASH_MAP_NR 4093

/** size of string pool in bytes */
#define POOL_SIZE 65536

/** A path component. Directory name are stored as a stack of path component.
 * Note than the name index acts also as an unique identifier */
struct op_hash_index {
	/** index inside the string pool */
	uint name;
	/** parent component, zero if this component is the root */
	uint parent;
} __attribute__((__packed__));

/** size of hash map in bytes */
#define OP_HASH_MAP_SIZE (OP_HASH_MAP_NR * sizeof(struct op_hash_index) + POOL_SIZE)


/* op_events.c: stuff needed by oprof_start and opf_filter.cpp */

/** Describe an event. */
struct op_event {
	uint counter_mask;	/**< bitmask of allowed counter  */
	u16  cpu_mask;		/**< bitmask of allowed cpu_type */
	u8 val;			/**< event number */
	/* FIXME, is u8 really sufficient ? */
	u8 unit;		/**< which unit mask if any allowed */
	const char *name;	/**< the event name */
	int min_count;		/**< minimum counter value allowed */
};

/** Describe an unit mask type. Events can optionnaly use a filter called
 * the unit mask. the mask type can be a bitmask or a discrete value */
enum unit_mask_type {
	utm_mandatory,		/**< useless but required by the hardware */
	utm_exclusive,		/**< only one of the values is allowed */
	utm_bitmask		/**< bitmask */
};

/** Describe an unit mask. */
struct op_unit_mask {
	uint num;		/**< number of possible unit masks */
	enum unit_mask_type unit_type_mask;
	u8 default_mask;	/**< only the gui use it */
	/* FIXME, is u8 really sufficient ? */
	u8 um[7];		/**< up to seven allowed unit masks */
};

/** Human readable description for an unit mask. */
struct op_unit_desc {
	char *desc[7];
};

#ifdef __cplusplus
extern "C" {
#endif

/* op_events.c */
int op_min_count(u8 ctr_type, op_cpu cpu_type);
int op_check_events(int ctr, u8 ctr_type, u8 ctr_um, op_cpu cpu_type);
const char* op_get_cpu_type_str(op_cpu cpu_type);
uint op_get_cpu_nr_counters(op_cpu cpu_type);
void op_get_event_desc(op_cpu cpu_type, u8 type, u8 um, char **typenamep, char **typedescp, char **umdescp);
op_cpu op_get_cpu_type(void);
int op_check_unit_mask(struct op_unit_mask *allow, u8 um);

#ifdef __cplusplus
}
#endif

extern struct op_unit_mask op_unit_masks[];
extern struct op_unit_desc op_unit_descs[];
extern char *op_event_descs[];
extern struct op_event op_events[];
/* the total number of events for all processor type */
extern uint op_nr_events;

#define CTR_ALL		(~0u)

#endif /* OP_USER_H */
