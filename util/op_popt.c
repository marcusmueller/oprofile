/* COPYRIGHT (C) 2000 THE VICTORIA UNIVERSITY OF MANCHESTER and John Levon
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <stdlib.h>

#include "op_popt.h"

/**
 * opd_poptGetContext - wrapper for popt
 *
 * Use this instead of poptGetContext to cope with
 * different popt versions. This also handle unrecognized
 * options. All error are fatal
 */
poptContext opd_poptGetContext(const char * name,
		int argc, const char ** argv,
		const struct poptOption * options, int flags)
{
	poptContext optcon;
	int c;

#ifdef CONST_POPT
	optcon = poptGetContext(name, argc, argv, options, flags); 
#else
	optcon = poptGetContext((char *)name, argc, (char **)argv, options, flags); 
#endif

	c = poptGetNextOpt(optcon);

	if (c < -1) {
		fprintf(stderr, "%s: %s: %s\n", argv[0],
			poptBadOption(optcon, POPT_BADOPTION_NOALIAS),
			poptStrerror(c));
		poptPrintHelp(optcon, stderr, 0);
		exit(EXIT_FAILURE);
	}

	return optcon;
}
