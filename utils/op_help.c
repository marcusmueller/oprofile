/**
 * @file op_help.c
 *
 * Adapted from libpperf 0.5 by M. Patrick Goda and Michael S. Warren
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 * 
 * @author John Levon <moz@compsoc.man.ac.uk>
 * @author Philippe Elie <phil_el@wanadoo.fr>
 */

/* See IA32 Vol. 3 Appendix A */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "version.h"
#include "op_events.h"
#include "op_events_desc.h"
#include "op_popt.h"

static op_cpu cpu_type = CPU_NO_GOOD;

/**
 * help_for_event - output event name and description
 * @param i  event number
 *
 * output an help string for the event @i
 */
static void help_for_event(int i)
{
	uint j,k;
	op_cpu cpu;
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
	for (cpu = 0; cpu < MAX_CPU_TYPE; ++cpu) {
		if (mask & (1 << cpu)) {
			printf("%s", op_get_cpu_type_str(cpu));
			mask &= ~(1 << cpu);
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
 * @param argc  program arg count
 * @param argv  program arg array
 *
 * Process the arguments, fatally complaining on error.
 */
static void get_options(int argc, char const * argv[])
{
	poptContext optcon;

	optcon = op_poptGetContext(NULL, argc, argv, options, 0);

	if (showvers) {
		show_version(argv[0]);
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
