/**
 * @file opd_printf.h
 * Output routines
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon <moz@compsoc.man.ac.uk>
 * @author Philippe Elie <phil_el@wanadoo.fr>
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
