/**
 * @file op_interface_25.h
 *
 * Module / user space interface for the new OProfile patch
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 */

#ifndef OP_INTERFACE_H
#define OP_INTERFACE_H

#include "op_config.h"
#include "op_types.h"

/** Data type to transfer samples counts from the module to the daemon */
struct op_sample {
	unsigned long cookie;
	off_t offset;
	unsigned long event;
} __attribute__((__packed__));

/**
 * The head structure of a kernel sample buffer.
 */
struct op_buffer_head {
	size_t count; /**< number of samples in this buffer */
	struct op_sample buffer[0]; /**< the sample buffer */
} __attribute__((__packed__));
	 
#endif /* OP_INTERFACE_H */
