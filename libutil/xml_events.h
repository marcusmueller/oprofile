/**
 * @file xml_utils.h
 * utility routines for generating XML
 *
 * @remark Copyright 2006 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Dave Nomura
 */

#ifndef XML_HELP_H
#define XML_HELP_H

#include "op_events.h"

void xml_help_for_event(struct op_event * event);
void open_xml_events(char * title, char * doc, op_cpu cpu_type);
void close_xml_events(void);

#endif /* !XML_HELP_H */

