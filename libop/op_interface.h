/**
 * @file op_interface.h
 *
 * Module / user space interface
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon <moz@compsoc.man.ac.uk>
 * @author Philippe Elie <phil_el@wanadoo.fr>
 */

#ifndef OP_INTERFACE_H
#define OP_INTERFACE_H

#include "op_config.h"
#include "op_types.h"

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
	u16 count; /**< samples count; high order bits contains the counter nr */
	u16 pid; /**< 32 bits but only 16 bits are used currently */
	u32 eip; /**< eip value where occur interrupt */
} __attribute__((__packed__, __aligned__(8)));

/** the current kernel-side profiler state */
enum oprof_state {
	STOPPED = 0,
	STOPPING = 1,
	RUNNING = 2
};
 
/**
 * The head structure of a kernel sample buffer.
 */
struct op_buffer_head {
	int cpu_nr; /**< the CPU number of this buffer */
	size_t count; /**< number of samples in this buffer */
	enum oprof_state state; /**< current profiler state */
	struct op_sample buffer[0]; /**< the sample buffer */
} __attribute__((__packed__));
	 
/**
 * Data type used by the module to notify daemon of fork/exit/mapping etc.
 * Meanings of fields depend on the type of notification encoded in the type
 * field.
 * \sa OP_FORK, OP_EXEC, OP_MAP, OP_DROP_MODULES and OP_EXIT
 */
struct op_note {
	u32 addr;
	u32 len;
	u32 offset;
	u32 hash;
	u16 pid;
	u16 type;
};

/**
 * A path component. Directory name are stored as a stack of path components.
 * Note than the name index acts also as an unique identifier
 */
struct op_hash_index {
	/** index inside the string pool */
	u32 name;
	/** parent component, zero if this component is the root */
	u32 parent;
} __attribute__((__packed__));

/** size of hash map in bytes */
#define OP_HASH_MAP_SIZE (OP_HASH_MAP_NR * sizeof(struct op_hash_index) + POOL_SIZE)

#endif /* OP_INTERFACE_H */
