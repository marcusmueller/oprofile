/**
 * @file daemon/opd_util.h
 * Code shared between 2.4 and 2.6 daemons
 *
 * @remark Copyright 2002, 2003 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 */

#ifndef OPD_UTIL_H

#include "op_cpu_type.h"

#include <signal.h>

/**
 * opd_options - parse command line options
 * @param argc  argc
 * @param argv  argv array
 *
 * Parse all command line arguments, and sanity
 * check what the user passed. Incorrect arguments
 * are a fatal error.
 */
void opd_options(int argc, char const * argv[]);


/**
 * opd_open_logfile - open the log file
 *
 * Open the logfile on stdout and stderr. This function
 * assumes that 1 and 2 are the lowest close()d file
 * descriptors. Failure to open on either descriptor is
 * a fatal error.
 */
void opd_open_logfile(void);

 
/**
 * opd_go_daemon - become daemon process
 *
 * Become an un-attached daemon in the standard
 * way (fork(),chdir(),setsid(),fork()).
 * Parents perform _exit().
 *
 * Any failure is fatal.
 */
void opd_go_daemon(void);


/**
 * opd_write_abi - write out the ABI description if needed
 */
void opd_write_abi(void);

/**
 * opd_hash_name - hash a name
 * @param name  name to hash
 *
 * return the hash code for the passed parameter name. FIXME: in libop/ or
 *  libutil/ ?
 */
size_t opd_hash_name(char const * name);


/**
 * is_image_filtered - check if we must profile this image
 * @param name  the name to check
 *
 * return non zero if name has been specified through --image= option or user
 * didn't ask for image filtering
 */
int is_image_filtered(char const * name);


/**
 * opd_setup_signals - setup signal handler
 */
void opd_setup_signals(void);

/** global variable positioned by signal handler */
extern sig_atomic_t signal_alarm;
extern sig_atomic_t signal_hup;
extern sig_atomic_t signal_term;
extern sig_atomic_t signal_usr1;
extern sig_atomic_t signal_usr2;

/** event description for setup (perfmon) and mangling */
struct opd_event {
	char * name;
	unsigned long value;
	unsigned long counter;
	unsigned long count;
	unsigned long um;
	unsigned long kernel;
	unsigned long user;
};

extern struct opd_event opd_events[];

extern double cpu_speed;
extern unsigned int op_nr_counters;
extern int verbose;
extern op_cpu cpu_type;
extern int separate_lib;
extern int separate_kernel;
extern int separate_thread;
extern int separate_cpu;
extern int no_vmlinux;
extern char * vmlinux;
extern char * kernel_range;

#endif /* OPD_UTIL_H */
