/**
 * @file dae/opd_printf.h
 * Output routines
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 */

#ifndef OPD_PRINTF_H
#define OPD_PRINTF_H

extern int verbose;

#define verbprintf(args...) \
        do { \
		if (verbose) \
			printf(args); \
	} while (0)

#endif /* OPD_PRINTF_H */
