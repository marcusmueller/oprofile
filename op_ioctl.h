/* $Id: op_ioctl.h,v 1.2 2000/12/05 01:03:25 moz Exp $ */

#ifndef OP_IOCTL_H
#define OP_IOCTL_H

/* FIXME: I knew this was too gross. If we ever support Pentium 4,
 * we will have to use struct-passing, as we have 18 counters amongst
 * other things.
 */

enum {

/* wake up for reading all the entries in the
 * eviction buffer
 */
OPROF_DUMP=1,
 
/* start profiling
 */
OPROF_START,
 
/* stop profiling
 */
OPROF_STOP,
 
/* The following ioctls set up profiling. They cannot be
 * used when profiling is in progress (do OPROF_STOP first),
 * otherwise a return value of EBUSY will occur.
 */

/* OPROF_SET_HASH_SIZE
 * arg: size of hash table in entries (each 32 bytes wide)
 * EFAULT: the memory could not be allocated
 */
OPROF_SET_HASH_SIZE,
 
/* OPROF_SET_BUF_SIZE
 * arg: size of eviction buffer in samples (each 8 bytes wide)
 * EFAULT: the memory could not be allocated
 */
OPROF_SET_BUF_SIZE,
 
/* OPROF_SET_PID_FILTER
 * arg: the pid to filter for. Any information for other processes is
 *      ignored.
 * EINVAL: The feature was not compiled in
 */
OPROF_SET_PID_FILTER,
 
/* OPROF_SET_PGRP_FILTER
 * arg: the pgrp to filter for. Any information for other processes is
 *      ignored.
 * EINVAL: The feature was not compiled in
 */
OPROF_SET_PGRP_FILTER,
 
/* OPROF_SET_CTR0
 * arg: logical CPU number to enable counter 0
 *      for. If the top bit is 1, the counter is enabled, otherwise
 *      it is disabled.
 * EINVAL: the CPU number was out of range 
 */
OPROF_SET_CTR0,

/* OPROF_SET_CTR1
 * arg: logical CPU number to enable counter 0
 *      for. If the top bit is 1, the counter is enabled, otherwise
 *      it is disabled.
 * EINVAL: the CPU number was out of range 
 */
OPROF_SET_CTR1,
 
/* OPROF_SET_CTR0_VAL
 * arg: the numeric event value to set counter 0 to count. The top two
 *      bytes identify the logical CPU number to set the counter for.
 * EINVAL: the CPU number was out of range.
 */ 
OPROF_SET_CTR0_VAL,
 
/* OPROF_SET_CTR1_VAL
 * arg: the numeric event value to set counter 1 to count. The top two
 *      bytes identify the logical CPU number to set the counter for.
 * EINVAL: the CPU number was out of range.
 */ 
OPROF_SET_CTR1_VAL,
 
/* OPROF_SET_CTR0_UM
 * arg: the numeric unit mask to set counter 0 to count. The top two
 *      bytes identify the logical CPU number to set the counter for.
 * EINVAL: the CPU number was out of range.
 */ 
OPROF_SET_CTR0_UM,
 
/* OPROF_SET_CTR1_UM
 * arg: the numeric unit mask to set counter 1 to count. The top two
 *      bytes identify the logical CPU number to set the counter for.
 * EINVAL: the CPU number was out of range.
 */ 
OPROF_SET_CTR1_UM,

/* OPROF_SET_CTR0_COUNT
 * arg: the counter value to reset to each time for counter 0. The top two
 *      bytes identify the logical CPU number to set the counter for.
 * EINVAL: the CPU number was out of range.
 */
OPROF_SET_CTR0_COUNT,
 
/* OPROF_SET_CTR1_COUNT
 * arg: the counter value to reset to each time for counter 1. The top two
 *      bytes identify the logical CPU number to set the counter for.
 * EINVAL: the CPU number was out of range.
 */
OPROF_SET_CTR1_COUNT,
 
/* OPROF_SET_CTR0_OS_USR:
 * arg: set when to profile. 2 means user-space only, 1 means kernel only, 
 *      0 means both. The top two bytes identify the logical CPU number to 
 *      set the counter for.
 * EINVAL: arg not 0, 1, or 2, or the CPU number was out of range. 
 */ 
OPROF_SET_CTR0_OS_USR,
 
/* OPROF_SET_CTR1_OS_USR:
 * arg: set when to profile. 2 means user-space only, 1 means kernel only, 
 *      0 means both. The top two bytes identify the logical CPU number to 
 *      set the counter for.
 * EINVAL: arg not 0, 1, or 2, or the CPU number was out of range. 
 */ 
OPROF_SET_CTR1_OS_USR,

};

#endif /* OP_IOCTL_H */
