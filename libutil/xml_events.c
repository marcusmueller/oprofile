#include <stdio.h>
#include <string.h>
#include "op_events.h"
#include "op_list.h"
#include "op_cpu_type.h"
#include "xml_out.h"

static op_cpu cpu_type;
#define MAX_BUFFER 1024
static char buffer[MAX_BUFFER];

void open_xml_events(char * title, char * doc, op_cpu the_cpu_type)
{
	char * schema_version = "1.0";

	buffer[0] = 0;
	cpu_type = the_cpu_type;
	open_xml_element(HELP_EVENTS, 0, buffer);
	open_xml_element(HELP_HEADER, 1, buffer);
	init_xml_str_attr(HELP_TITLE, title, buffer);
	init_xml_str_attr(SCHEMA_VERSION, schema_version, buffer);
	init_xml_str_attr(HELP_DOC, doc, buffer);
	close_xml_element(NONE, 0, buffer);
	printf("%s", buffer);
}

void close_xml_events(void)
{
	buffer[0] = 0;
	close_xml_element(HELP_EVENTS, 0, buffer);
	printf("%s", buffer);
}

static void
xml_do_arch_specific_event_help(struct op_event * event, char * buffer)
{
	switch (cpu_type) {
	case CPU_PPC64_CELL:
		init_xml_int_attr(HELP_EVENT_GROUP, event->val / 100, buffer);
		break;
	default:
		break;
	}
}


void xml_help_for_event(struct op_event * event)
{
	uint j;
	int nr_counters;
	int has_nested = strcmp(event->unit->name, "zero");

	buffer[0] = 0;
	open_xml_element(HELP_EVENT, 1, buffer);
	init_xml_str_attr(HELP_EVENT_NAME, event->name, buffer);
	xml_do_arch_specific_event_help(event, buffer);
	init_xml_str_attr(HELP_EVENT_DESC, event->desc, buffer);

	nr_counters = op_get_nr_counters(cpu_type);
	init_xml_int_attr(HELP_COUNTER_MASK, event->counter_mask, buffer);
	init_xml_int_attr(HELP_MIN_COUNT, event->min_count, buffer);

	if (has_nested) {
		close_xml_element(NONE, 1, buffer);
		open_xml_element(HELP_UNIT_MASKS, 1, buffer);
		init_xml_int_attr(HELP_DEFAULT_MASK,
				  event->unit->default_mask, buffer);
		close_xml_element(NONE, 1, buffer);
		for (j = 0; j < event->unit->num; j++) {
			open_xml_element(HELP_UNIT_MASK, 1, buffer);
			init_xml_int_attr(HELP_UNIT_MASK_VALUE,
					  event->unit->um[j].value, buffer);
			init_xml_str_attr(HELP_UNIT_MASK_DESC,
					  event->unit->um[j].desc, buffer);
			close_xml_element(NONE, 0, buffer);
		}
		close_xml_element(HELP_UNIT_MASKS, 0, buffer);
	}
	close_xml_element(has_nested ? HELP_EVENT : NONE, has_nested, buffer);
	printf("%s", buffer);
}

