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

#include <signal.h>

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
 * opd_setup_signals - setup signal handler
 */
void opd_setup_signals(void);

/** global variable positionned by signal handler */
extern sig_atomic_t signal_alarm;
extern sig_atomic_t signal_hup;
extern sig_atomic_t signal_term;

#endif /* OPD_UTIL_H */
