/**
 * @file op_config.h
 *
 * Parameters a user may want to change. See
 * also the relevant op_config_2[45].h
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 */

#ifndef OP_CONFIG_H
#define OP_CONFIG_H

/* various paths, duplicated in op_start */
#define OP_BASE_DIR "/var/lib/oprofile/"
#define OP_SAMPLES_DIR OP_BASE_DIR "samples/"
#define OP_LOCK_FILE OP_BASE_DIR "lock"
#define OP_LOG_FILE OP_BASE_DIR "oprofiled.log"

#define OPD_MAGIC "DAE\n"
#define OPD_VERSION 0x8

#ifndef NR_CPUS
/** maximum number of cpus present in the box */
#define NR_CPUS 32
#endif

/** maximum number of profilable kernel modules */
#define OPD_MAX_MODULES 64

#endif /* OP_CONFIG_H */
