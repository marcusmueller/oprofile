/* $Id: op_help.c,v 1.3 2001/12/01 21:16:48 phil_e Exp $ */
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

/* Adapted from libpperf 0.5 by M. Patrick Goda and Michael S. Warren */

/* See IA32 Vol. 3 Appendix A */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "../op_user.h"
#include "../util/op_popt.h"

static int cpu_type = CPU_NO_GOOD;

/**
 * help_for_event - output event name and description
 * @i: event number
 *
 * output an help string for the event @i
 */
static void help_for_event(int i)
{
	uint k, j;
	uint mask;

	printf("%s", op_events[i].name);

	printf(": (counter: ");
	if (op_events[i].counter_mask == CTR_ALL) {
		printf("all");
	} else {
		mask = op_events[i].counter_mask;
		for (k = 0; k < CHAR_BIT * sizeof(op_events[i].counter_mask); ++k) {
			if (mask & (1 << k)) {
				printf("%d", k);
				mask &= ~(1 << k);
				if (mask)
					printf(", ");
			}
		}
	}
	printf(")");

	printf(" (supported cpu: ");
	mask = op_events[i].cpu_mask;
	for (k = 0; k < MAX_CPU_TYPE; ++k) {
		if (mask & (1 << k)) {
			printf("%s", op_get_cpu_type_str(k));
			mask &= ~(1 << k);
			if (mask)
				printf(", ");
		}
	}

	printf(")\n\t%s (min count: %d)\n", op_event_descs[i], op_events[i].min_count);

	if (op_events[i].unit) {
		int unit_idx = op_events[i].unit;

		printf("\tUnit masks\n");
		printf("\t----------\n");

		for (j=0; j < op_unit_masks[unit_idx].num; j++) {
			printf("\t%.2x: %s\n",
			       op_unit_masks[unit_idx].um[j],
			       op_unit_descs[unit_idx].desc[j]);
		}
	}
}

static int showvers;
static int cpu_type;
static int get_cpu_type;
static const char * event_name;

static struct poptOption options[] = {
	{ "cpu-type", 'c', POPT_ARG_INT, &cpu_type, 0,
	  "force cpu type", "cpu type", },
	{ "get-cpu-type", 'r', POPT_ARG_NONE, &get_cpu_type, 0,
	  "show the auto detected cpu type", NULL, },
	{ "version", 'v', POPT_ARG_NONE, &showvers, 0, "show version", NULL, },
	POPT_AUTOHELP
	{ NULL, 0, 0, NULL, 0, NULL, NULL, },
};

/**
 * get_options - process command line
 * @argc: program arg count
 * @argv: program arg array
 *
 * Process the arguments, fatally complaining on error.
 */
static void get_options(int argc, char const * argv[])
{
	poptContext optcon;

	optcon = opd_poptGetContext(NULL, argc, argv, options, 0);

	if (showvers) {
		printf("%s : " VERSION_STRING " compiled on "
		       __DATE__ " " __TIME__ "\n", argv[0]);
		exit(EXIT_SUCCESS);
	}

	/* non-option, must be a valid event name */
	event_name = poptGetArg(optcon);

	poptFreeContext(optcon);
}

int main(int argc, char const *argv[])
{
	int cpu_type_mask;
	uint j;

	cpu_type = op_get_cpu_type();

	get_options(argc, argv);

	if (cpu_type < 0 || cpu_type >= MAX_CPU_TYPE) {
		fprintf(stderr, "invalid cpu type %d !\n", cpu_type);
		exit(EXIT_FAILURE);
	}

	if (get_cpu_type) {
		printf("%d\n", cpu_type);
		exit(EXIT_SUCCESS);
	}

	if (event_name) {
		cpu_type_mask = 1 << cpu_type;
		for (j=0; j < op_nr_events; j++) {
			if (!strcmp(op_events[j].name, event_name) && 
			    (op_events[j].cpu_mask & cpu_type_mask)) {
				printf("%d\n", op_events[j].val); 
				exit(EXIT_SUCCESS);
			}
		}
		fprintf(stderr, "No such event \"%s\"\n", event_name);
		exit(EXIT_FAILURE);
	} 

	printf("oprofile: available events\n");
	printf("--------------------------\n\n");
	if (cpu_type == CPU_ATHLON)
		printf ("See AMD document x86 optimisation guide (22007.pdf), Appendix D\n\n");
	else
		printf("See Intel Architecture Developer's Manual\nVol. 3 (), Appendix A\n\n");

	cpu_type_mask = 1 << cpu_type;
	for (j = 0; j < op_nr_events; j++) {
		if ((op_events[j].cpu_mask & cpu_type_mask) != 0) {
			help_for_event(j);
		}
	}

	return EXIT_SUCCESS;
}
