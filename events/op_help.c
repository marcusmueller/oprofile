/* $Id: op_help.c,v 1.2 2001/11/26 21:44:35 phil_e Exp $ */
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

int main(int argc, char *argv[])
{
	int i;
	uint j;
	int cpu_type_mask;
	int for_gui;

	cpu_type = op_get_cpu_type();

	for_gui = 0;
	for (i = 1 ; i < argc ; ++i) {
		if (!strcmp(argv[i], "--version")) {
			printf(VERSION_STRING " compiled on " __DATE__ " " __TIME__ "\n");
			return 0;
		} else if (!strcmp(argv[i], "--help")) {
			printf("op_help [--version|--cpu-type] event_name\n");
			return 0;
		} else if (!strncmp(argv[i], "--cpu-type=", 11)) {
			sscanf(argv[i] + 11, "%d", &cpu_type);
			if (cpu_type < 0 || cpu_type >= MAX_CPU_TYPE) {
				fprintf(stderr, "invalid cpu type %d !\n", cpu_type);
				exit(EXIT_FAILURE);
			}
		} else if (!strncmp(argv[i], "--get-cpu-type", 11)) {
			printf("%d\n", cpu_type);
			exit(EXIT_SUCCESS);
		} else {
			cpu_type_mask = 1 << cpu_type;
			for (j=0; j < op_nr_events; j++) {
				if (!strcmp(op_events[j].name, argv[i]) && 
				    (op_events[j].cpu_mask & cpu_type_mask)) {
					printf("%d\n", op_events[j].val); 
					return 0;
				}
			}
			fprintf(stderr, "No such event \"%s\"\n", argv[i]);
			return 1;
		return 0;
		}
	}

	printf("oprofile: available events\n");
	printf("--------------------------\n\n");
	if (cpu_type == CPU_ATHLON)
		printf ("See AMD document x86 optimisation guide (22007.pdf), Appendix D\n\n");
	else
		printf("See Intel Architecture Developer's Manual\nVol. 3, Appendix A\n\n");

	cpu_type_mask = 1 << cpu_type;
	for (j=0; j < op_nr_events; j++) {
		if ((op_events[j].cpu_mask & cpu_type_mask) != 0) {
			help_for_event(j);
		}
	}

	return 0;
}
