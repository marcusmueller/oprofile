/* $Id: op_user.h,v 1.24 2002/05/05 04:21:54 phil_e Exp $ */
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

#include "version.h"

/* various paths, duplicated in op_start */
#define OP_BASE_DIR "/var/lib/oprofile/"
#define OP_SAMPLES_DIR OP_BASE_DIR "samples/"
#define OP_LOCK_FILE OP_BASE_DIR "lock"
#define OP_DEVICE OP_BASE_DIR "opdev"
#define OP_NOTE_DEVICE OP_BASE_DIR "opnotedev"
#define OP_HASH_DEVICE OP_BASE_DIR "ophashmapdev"
#define OP_LOG_FILENAME "oprofiled.log"
#define OP_LOG_FILE OP_BASE_DIR OP_LOG_FILENAME
 
/*@{\name miscellaneous types */
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
/*@}*/

/*@{\name op_check_events() return code */
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
/** maximum number of cpus present in the box */
#define NR_CPUS 32
#endif 

/*@{\name module default/min/max settings */

/** 65536 * 32 = 2097152 bytes default */
#define OP_DEFAULT_HASH_SIZE 65536
/** maximum number of entry in module samples hash table */
#define OP_MAX_HASH_SIZE 262144
/** minimum number of entry in module samples hash table */
#define OP_MIN_HASH_SIZE 256

/** 32768 * 8 = 262144 bytes default */
#define OP_DEFAULT_BUF_SIZE 32768
/** we don't try to wake-up daemon until it remains more than this free entry
 * in eviction buffer */
#define OP_PRE_WATERMARK 2048
/** maximum number of entry in samples eviction buffer */
#define OP_MAX_BUF_SIZE	1048576
/** minimum number of entry in samples eviction buffer */
#define OP_MIN_BUF_SIZE	(1024 + OP_PRE_WATERMARK)

/** 16384 * sizeof(op_note) = 273680 bytes default */
#define OP_DEFAULT_NOTE_SIZE 16384
/** we don't try to wake-up daemon until it remains more than this free entry
 * in note buffer */
#define OP_PRE_NOTE_WATERMARK	512
/** maximum number of entry in note buffer */
#define OP_MAX_NOTE_TABLE_SIZE	1048576
/** minimum number of entry in note buffer */
#define OP_MIN_NOTE_TABLE_SIZE	(1024 + OP_PRE_NOTE_WATERMARK)

/** maximum number of events between interrupts. Counters are 40 bits, but
 * for convenience we only use 32 bits. The top bit is used for overflow
 * detection, so user can set up to (2^31)-1 */
#define OP_MAX_PERF_COUNT	2147483647UL

/** maximum sampling rate when using RTC */
#define OP_MAX_RTC_COUNT	4096
/** minimum sampling rate when using RTC */
#define OP_MIN_RTC_COUNT	2

/*@}*/

/** maximum number of counters, up to 4 for Athlon (18 for P4). The primary
 * use of this variable is for static/local array dimension. Never use it in 
 * loop or in array index access/index checking unless you know what you
 * made. Don't change it without updating OP_BITS_CTR! */
#define OP_MAX_COUNTERS	4

/** the number of bits neccessary to store OP_MAX_COUNTERS values */
#define OP_BITS	2

/** The number of bits available to store count. The 16 value is
 * sizeof_in_bits(op_sample.count)  */
#define OP_BITS_COUNT	(16 - OP_BITS)

/** counter nr mask */
#define OP_CTR_MASK	((~0U << (OP_BITS_COUNT + 1)) >> 1)

/** top OP_BITS bits of count are used to store counter number */
#define OP_COUNTER(x)	(((x) & OP_CTR_MASK) >> OP_BITS_COUNT)
/** low bits store the counter value */
#define OP_COUNT_MASK	((1U << OP_BITS_COUNT) - 1U)

/*@{\name notifications types encoded in op_note::type */
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
/*@}*/

/** Data type to transfer samples counts from the module to the daemon */
struct op_sample {
	/** samples count; high order bits contains the counter nr */
	u16 count;
	u16 pid;	/**< 32 bits but only 16 bits are used currently */
	u32 eip;	/**< eip value where occur interrupt */
} __attribute__((__packed__, __aligned__(8)));

/** Data type used by the module to notify daemon of fork/exit/mapping etc.
 * Meanings of fields depend on the type of notification encoded in the type
 * field.
 * \sa OP_FORK, OP_EXEC, OP_MAP, OP_DROP_MODULES and OP_EXIT */
struct op_note {
	u32 addr;
	u32 len;
	u32 offset;
	uint hash;
	u16 pid;
	u16 type;  /* FIXME how to put a see also to the group OP_FORK etc.*/
};

/** nr entries in hash map. This is the maximum number of name components
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
/**
 * \param ctr_type event value
 * \param cpu_type cpu type
 *
 * The function returns > 0 if the event is found 0 otherwise
 */
int op_min_count(u8 ctr_type, op_cpu cpu_type);

/**
 * sanity check event values
 * \param ctr counter number
 * \param ctr_type event value for counter 0
 * \param ctr_um unit mask for counter 0
 * \param cpu_type processor type
 *
 * Check that the counter event and unit mask values are allowed.
 *
 * Don't fail if ctr_type == 0. (FIXME why, this seems insane)
 *
 * The function returns bitmask of failure cause 0 otherwise
 *
 * \sa op_cpu, OP_EVENTS_OK
 */
int op_check_events(int ctr, u8 ctr_type, u8 ctr_um, op_cpu cpu_type);

/**
 * get the cpu string.
 * \param cpu_type the cpu type identifier
 *
 * The function always return a valid const char* the core cpu denomination
 * or "invalid cpu type" if cpu_type is not valid.
 */
const char* op_get_cpu_type_str(op_cpu cpu_type);

/**
 * get the number of counter available for this cpu
 * \param cpu_type the cpu type identifier
 *
 * The function return the number of counter available for this
 * cpu type. return (uint)-1 if the cpu type is nopt recognized
 */
uint op_get_cpu_nr_counters(op_cpu cpu_type);

/**
 * sanity check unit mask value
 * \param allow allowed unit mask array
 * \param um unit mask value to check
 *
 * Verify that a unit mask value is within the allowed array.
 *
 * The function returns:
 * -1  if the value is not allowed,
 * 0   if the value is allowed and represent multiple units,
 * > 0 otherwise.
 *
 * if the return value is > 0 caller can access to the description of
 * the unit_mask through op_unit_descs
 * \sa op_unit_descs
 */
int op_check_unit_mask(struct op_unit_mask *allow, u8 um);

/* op_events_desc.c */

/**
 * get event name and description
 * \param cpu_type the cpu_type
 * \param type event value
 * \param um unit mask
 * \param typenamep returned event name string
 * \param typedescp returned event description string
 * \param umdescp returned unit mask description string
 *
 * Get the associated event name and descriptions given
 * the cpu type, event value and unit mask value. It is a fatal error
 * to supply a non-valid type value, but an invalid um will not exit.
 *
 * typenamep, typedescp, umdescp are filled in with pointers
 * to the relevant name and descriptions. umdescp can be set to
 * NULL when um is invalid for the given type value.
 * These strings are static and should not be freed.
 */
void op_get_event_desc(op_cpu cpu_type, u8 type, u8 um,
		       char **typenamep, char **typedescp, char **umdescp);

/**
 * get from /proc/sys/dev/oprofile/cpu_type the cpu type
 *
 * returns CPU_NO_GOOD if the CPU could not be identified.
 * This function can not work if the module is not loaded
 */
op_cpu op_get_cpu_type(void);

#ifdef __cplusplus
}
#endif

/** unit mask description */
extern struct op_unit_mask op_unit_masks[];
/** unit mask string description */
extern struct op_unit_desc op_unit_descs[];
/** events string description */
extern char *op_event_descs[];
/** description of events for all processor type */
extern struct op_event op_events[];
/** the total number of events for all processor type, allowing to iterate
 * on the op_events[] decription */
extern uint op_nr_events;

/** a special constant meanings, this events is available for all counters */
#define CTR_ALL		(~0u)

#endif /* OP_USER_H */
