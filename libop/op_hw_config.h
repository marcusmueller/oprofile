/**
 * \file op_hw_config.h
 * Copyright 2002 OProfile authors
 * Read the file COPYING
 *
 * \author John Levon <moz@compsoc.man.ac.uk>
 * \author Philippe Elie <phil_el@wanadoo.fr>
 */

#ifndef OP_HW_CONFIG_H
#define OP_HW_CONFIG_H

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

/** maximum number of events between interrupts. Counters are 40 bits, but
 * for convenience we only use 32 bits. The top bit is used for overflow
 * detection, so user can set up to (2^31)-1 */
#define OP_MAX_PERF_COUNT	2147483647UL

#endif /* OP_HW_CONFIG_H */
