/**
 * @file op_hw_config.h
 * Configuration parameters that are dependent on CPU/architecture
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon <moz@compsoc.man.ac.uk>
 * @author Philippe Elie <phil_el@wanadoo.fr>
 */

#ifndef OP_HW_CONFIG_H
#define OP_HW_CONFIG_H

/** maximum number of counters, up to 4 for Athlon (18 for P4). The primary
 * use of this variable is for static/local array dimension. Never use it in
 * loop or in array index access/index checking unless you know what you
 * made. Don't change it without updating OP_BITS_CTR! */
#define OP_MAX_COUNTERS	4

/** a plain unsigned int magic value to check against counter overflow */
#define OP_COUNT_MAX	~0u

/** maximum number of events between interrupts. Counters are 40 bits, but
 * for convenience we only use 32 bits. The top bit is used for overflow
 * detection, so user can set up to (2^31)-1 */
#define OP_MAX_PERF_COUNT	2147483647UL

#endif /* OP_HW_CONFIG_H */
