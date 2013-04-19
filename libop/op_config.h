/**
 * @file op_config.h
 *
 * Parameters a user may want to change. See
 * also op_config_24.h
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 * @Modifications Daniel Hansel
 */

#ifndef OP_CONFIG_H
#define OP_CONFIG_H

#if defined(__cplusplus)
extern "C" {
#endif
  

enum {	OPERF_SAMPLES, /**< nr. samples */
	OPERF_KERNEL, /**< nr. kernel samples */
	OPERF_PROCESS, /**< nr. userspace samples */
	OPERF_INVALID_CTX, /**< nr. samples lost due to sample address not in expected range for domain */
	OPERF_LOST_KERNEL,  /**< nr. kernel samples lost */
	OPERF_LOST_SAMPLEFILE, /**< nr samples for which sample file can't be opened */
	OPERF_LOST_NO_MAPPING, /**< nr samples lost due to no mapping */
	OPERF_NO_APP_KERNEL_SAMPLE, /**<nr. user ctx kernel samples dropped due to no app context available */
	OPERF_NO_APP_USER_SAMPLE, /**<nr. user samples dropped due to no app context available */
	OPERF_BT_LOST_NO_MAPPING, /**<nr. backtrace samples dropped due to no mapping */
	OPERF_LOST_INVALID_HYPERV_ADDR, /**<nr. hypervisor samples dropped due to address out-of-range */
	OPERF_RECORD_LOST_SAMPLE, /**<nr. samples lost reported by perf_events kernel */
	OPERF_MAX_STATS /**< end of stats */
};
#define OPERF_INDEX_OF_FIRST_LOST_STAT 3

/* Warn on lost samples if number of lost samples is greater the this fraction
 * of the total samples
*/
#define OPERF_WARN_LOST_SAMPLES_THRESHOLD   0.0001

extern char * stats_filenames[];

/** 
 * must be called to initialize the paths below.
 * @param session_dir  the non-NULL value of the base session directory
 */
void init_op_config_dirs(char const * session_dir);

//void init_op_stats_filenames(char const * session_dir);

#define OP_SESSION_DIR_DEFAULT "/var/lib/oprofile/"


/*@{\name module default/min/max settings */

/** 65536 * sizeof(op_sample) */
#define OP_DEFAULT_BUF_SIZE 65536
/**
 * we don't try to wake-up daemon until it remains more than this free entry
 * in eviction buffer
 */
#define OP_PRE_WATERMARK(buffer_size)			\
	(((buffer_size) / 8) < OP_MIN_PRE_WATERMARK	\
		? OP_MIN_PRE_WATERMARK			\
		: (buffer_size) / 8)
/** minimal buffer water mark before we try to wakeup daemon */
#define OP_MIN_PRE_WATERMARK 8192
/** maximum number of entry in samples eviction buffer */
#define OP_MAX_BUF_SIZE	1048576
/** minimum number of entry in samples eviction buffer */
#define OP_MIN_BUF_SIZE	(32768 + OP_PRE_WATERMARK(32768))

/** maximum sampling rate when using RTC */
#define OP_MAX_RTC_COUNT	4096

/* 
 * various paths, corresponding to opcontrol, that should be
 * initialized by init_op_config_dirs() above. 
 */
extern char op_session_dir[];
extern char op_samples_dir[];
extern char op_samples_current_dir[];
extern char op_lock_file[];
extern char op_log_file[];
extern char op_pipe_file[];
extern char op_dump_status[];

/* Global directory that stores debug files */
#ifndef DEBUGDIR
#define DEBUGDIR "/usr/lib/debug"
#endif

#define OPD_MAGIC "DAE\n"
#define OPD_VERSION 0x12

#define OP_MIN_CPU_BUF_SIZE 2048
#define OP_MAX_CPU_BUF_SIZE 131072

#if defined(__cplusplus)
}
#endif

#endif /* OP_CONFIG_H */
