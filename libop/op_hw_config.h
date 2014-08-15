/**
 * @file op_hw_config.h
 * Configuration parameters that are dependent on CPU/architecture
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 */

#ifndef OP_HW_CONFIG_H
#define OP_HW_CONFIG_H

/** Maximum number of counters.
 * The primary use of this variable is for static/local array dimension.
 * Never use it in loops or in array index access/index checking unless
 * you know what you made. */
#define OP_MAX_COUNTERS	8

/** maximum number of events between interrupts. Counters are 40 bits, but
 * for convenience we only use 32 bits. The top bit is used for overflow
 * detection, so user can set up to (2^31)-1 */
#define OP_MAX_PERF_COUNT	2147483647UL

#endif /* OP_HW_CONFIG_H */
