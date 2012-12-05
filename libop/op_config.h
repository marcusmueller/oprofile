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
  
/** 
 * must be called to initialize the paths below.
 * @param session_dir  the non-NULL value of the base session directory
 */
void init_op_config_dirs(char const * session_dir);

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
