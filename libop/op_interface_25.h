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

#ifndef OP_INTERFACE_25_H
#define OP_INTERFACE_25_H

#include "op_config.h"
#include "op_types.h"

#define ESCAPE_CODE		~0UL
#define CTX_SWITCH_CODE		1
#define CPU_SWITCH_CODE		2
#define COOKIE_SWITCH_CODE	3
 
/**
 * The head structure of a kernel sample buffer.
 */
struct op_buffer_head {
	size_t count; /**< number of samples in this buffer */
	unsigned long buffer[0]; /**< the sample buffer */
} __attribute__((__packed__));
	 
#endif /* OP_INTERFACE_25_H */
