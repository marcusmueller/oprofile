/**
 * @file op_help.c
 * Print out PMC event information
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "op_version.h"
#include "op_events.h"
#include "op_popt.h"
#include "op_cpufreq.h"

static op_cpu cpu_type = CPU_NO_GOOD;

/**
 * help_for_event - output event name and description
 * @param i  event number
 *
 * output an help string for the event @i
 */
static void help_for_event(struct op_event * event)
{
	uint i, j;
	uint mask;

	printf("%s", event->name);

	printf(": (counter: ");

	/* FIXME, reintroduce
	if (event->counter_mask == CTR_ALL) {
		printf("all");
	} else {
	*/
		mask = event->counter_mask;
		for (i = 0; i < CHAR_BIT * sizeof(event->counter_mask); ++i) {
			if (mask & (1 << i)) {
				printf("%d", i);
				mask &= ~(1 << i);
				if (mask)
					printf(", ");
			}
		}
	/*}*/

	printf(")\n\t%s (min count: %d)\n", event->desc, event->min_count);

	if (strcmp(event->unit->name, "zero")) {

		printf("\tUnit masks\n");
		printf("\t----------\n");

		for (j=0; j < event->unit->num; j++) {
			printf("\t0x%.2x: %s\n",
			       event->unit->um[j].value,
			       event->unit->um[j].desc);
		}
	}
}

static int showvers;
static int get_cpu_type;
static int get_cpu_frequency;
static char const * event_name;

static struct poptOption options[] = {
	{ "cpu-type", 'c', POPT_ARG_INT, &cpu_type, 0,
	  "use the given numerical CPU type", "cpu type", },
	{ "get-cpu-type", 'r', POPT_ARG_NONE, &get_cpu_type, 0,
	  "show the auto-detected CPU type", NULL, },
	{ "get-cpu-frequency", '\0', POPT_ARG_NONE|POPT_ARGFLAG_DOC_HIDDEN,
	  &get_cpu_frequency, 0, "show the cpu frequency in MHz", NULL, },
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
	struct list_head * events;
	struct list_head * pos;
	char const * pretty;

	get_options(argc, argv);

	if (get_cpu_frequency) {
		printf("%f\n", op_cpu_frequency());
		exit(EXIT_SUCCESS);
	}

	if (cpu_type == CPU_NO_GOOD)
		cpu_type = op_get_cpu_type();

	if (cpu_type < 0 || cpu_type >= MAX_CPU_TYPE) {
		fprintf(stderr, "cpu_type '%d' is not valid\n", cpu_type);
		exit(EXIT_FAILURE);
	}

	pretty = op_get_cpu_type_str(cpu_type);

	if (get_cpu_type) {
		printf("%s\n", pretty);
		exit(EXIT_SUCCESS);
	}

	if (cpu_type == CPU_TIMER_INT) {
		printf("using timer interrupt\n");
		exit(event_name ? EXIT_FAILURE : EXIT_SUCCESS);
	}

	events = op_events(cpu_type);

	if (event_name) {
		list_for_each(pos, events) {
			struct op_event * event = list_entry(pos, struct op_event, event_next);

			if (strcmp(event->name, event_name) == 0) {
				printf("%d\n", event->val);
				exit(EXIT_SUCCESS);
			}
		}
		fprintf(stderr, "No such event \"%s\"\n", event_name);
		exit(EXIT_FAILURE);
	}

	printf("oprofile: available events for CPU type \"%s\"\n\n", pretty);
	switch (cpu_type) {
	case CPU_HAMMER:
		break;
	case CPU_ATHLON:
		printf ("See AMD document x86 optimisation guide (22007.pdf), Appendix D\n\n");
		break;
	case CPU_PPRO:
	case CPU_PII:
	case CPU_PIII:
	case CPU_P4:
	case CPU_P4_HT2:
		printf("See Intel Architecture Developer's Manual Volume 3, Appendix A and\n"
		"Intel Architecture Optimization Reference Manual (730795-001)\n\n");
		break;
	case CPU_IA64:
	case CPU_IA64_1:
	case CPU_IA64_2:
		printf("See Intel Itanium Processor Reference Manual\n"
		       "for Software Development (Document 245320-003),\n"
		       "Intel Itanium Processor Reference Manual\n"
		       "for Software Optimization (Document 245473-003),\n"
		       "Intel Itanium 2 Processor Reference Manual\n"
		       "for Software Development and Optimization (Document 251110-001),\n\n");
		break;
	case CPU_AXP_EV4:
	case CPU_AXP_EV5:
	case CPU_AXP_PCA56:
	case CPU_AXP_EV6:
	case CPU_AXP_EV67:
		printf("See Alpha Architecture Reference Manual\n"
		       "ftp://ftp.compaq.com/pub/products/alphaCPUdocs/alpha_arch_ref.pdf\n");
		break;
	case CPU_RTC:
		break;
	default:
		printf("%d is not a valid processor type,\n", cpu_type);
	}

	list_for_each(pos, events) {
		struct op_event * event = list_entry(pos, struct op_event, event_next);
		help_for_event(event);
	}

	return EXIT_SUCCESS;
}
