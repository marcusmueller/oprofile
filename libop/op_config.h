/**
 * \file op_config.h
 * Copyright 2002 OProfile authors
 * Read the file COPYING
 *
 * Parameters a user may want to change
 *
 * \author John Levon <moz@compsoc.man.ac.uk>
 * \author Philippe Elie <phil_el@wanadoo.fr>
 */

#ifndef OP_CONFIG_H
#define OP_CONFIG_H

/* various paths, duplicated in op_start */
#define OP_BASE_DIR "/var/lib/oprofile/"
#define OP_SAMPLES_DIR OP_BASE_DIR "samples/"
#define OP_LOCK_FILE OP_BASE_DIR "lock"
#define OP_DEVICE OP_BASE_DIR "opdev"
#define OP_NOTE_DEVICE OP_BASE_DIR "opnotedev"
#define OP_HASH_DEVICE OP_BASE_DIR "ophashmapdev"
#define OP_LOG_FILE OP_BASE_DIR "oprofiled.log"
 
#define OPD_MAGIC "DAE\n"
#define OPD_VERSION 0x6

#ifndef NR_CPUS
/** maximum number of cpus present in the box */
#define NR_CPUS 32
#endif 

/* This is a standard non-portable assumption we make. */
#define OP_MIN_PID		0
#define OP_MAX_PID		32767
#define OP_MIN_PGRP		0
#define OP_MAX_PGRP		32767

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

/** maximum sampling rate when using RTC */
#define OP_MAX_RTC_COUNT	4096
/** minimum sampling rate when using RTC */
#define OP_MIN_RTC_COUNT	2

/*@}*/

/** nr entries in hash map. This is the maximum number of name components
 * allowed. Must be a prime number */
#define OP_HASH_MAP_NR 4093

/** size of string pool in bytes */
#define POOL_SIZE 65536

#endif /* OP_CONFIG_H */
