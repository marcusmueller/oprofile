/**
 * @file xml_out.cpp
 * C utility routines for writing XML
 *
 * @remark Copyright 2008 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Dave Nomura
 */

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include "xml_out.h"

char * xml_tag_map[] = {
	"NONE",
	"id",
	"profile",
		"processor",
		"cputype",
		"title",
		"schemaversion",
		"mhz",
	"setup",
	"timersetup",
		"rtcinterrupts",
	"eventsetup",
		"eventname",
		"unitmask",
		"setupcount",
		"separatedcpus",
	"options",
		"session", "debuginfo", "details", "excludedependent", "excludesymbols",
		"imagepath", "includesymbols", "merge",
	"classes",
	"class",
		"cpu",
		"event",
		"mask",
	"process",
		"pid",
	"thread",
		"tid",
	"binary",
	"module",
		"name",
	"callers",
	"callees",
	"symbol",
		"idref",
		"self",
		"detaillo",
		"detailhi",
	"symboltable",
	"symboldata",
		"startingaddr",
		"file",
		"line",
		"codelength",
	"summarydata",
	"sampledata",
	"count",
	"detailtable",
	"symboldetails",
	"detaildata",
		"vmaoffset",
	"bytestable",
	"bytes",
	"help_events",
	"header",
		"title",
		"doc",
	"event",
		"event_name",
		"group",
		"desc",
		"counter_mask",
		"min_count",
	"unit_masks",
		"default",
	"unit_mask",
		"mask",
		"desc"
};

static char quote_buf[1024];
static char tmp_buf[1024];


char * xml_tag_name(tag_t tag)
{
	return xml_tag_map[tag];
}


void open_xml_element(tag_t tag, int with_attrs, char * buffer)
{

	if (sprintf(tmp_buf, "<%s%s", xml_tag_name(tag),
		   (with_attrs ? " " : ">\n")) < 0) {
			fprintf(stderr,"open_xml_element: sprintf failed: %s\n",
				strerror(errno));
			exit(EXIT_FAILURE);
	}
	strcat(buffer, tmp_buf);
}


void close_xml_element(tag_t tag, int has_nested, char * buffer)
{

	if (tag == NONE) {
		if (sprintf(tmp_buf, "%s\n", (has_nested ? ">" : "/>")) < 0) {
			fprintf(stderr,"close_xml_element: sprintf failed: %s\n",
				strerror(errno));
			exit(EXIT_FAILURE);
		}
	} else {
		if (sprintf(tmp_buf, "</%s>\n", xml_tag_name(tag)) < 0) {
			fprintf(stderr,"close_xml_element: sprintf failed: %s\n",
				strerror(errno));
			exit(EXIT_FAILURE);
		}
	}
	strcat(buffer, tmp_buf);
}


void init_xml_int_attr(tag_t attr, int value, char * buffer)
{


	if (sprintf(tmp_buf, " %s=\"%d\"", xml_tag_name(attr), value) < 0) {
			fprintf(stderr,"init_xml_int_attr: sprintf failed: %s\n",
				strerror(errno));
			exit(EXIT_FAILURE);
	}
	strcat(buffer, tmp_buf);
}


void init_xml_dbl_attr(tag_t attr, double value, char * buffer)
{

	if (sprintf(tmp_buf, " %s=\"%.2f\"", xml_tag_name(attr), value) < 0) {
			fprintf(stderr,"init_xml_dbL_attr: sprintf failed: %s\n",
				strerror(errno));
			exit(EXIT_FAILURE);
	}
	strcat(buffer, tmp_buf);
}


static char * quote(char const * str)
{

	int i;
	int pos = 0;
	int len = strlen(str);

	
	quote_buf[pos++] = '"';

	for (i = 0; i < len; i++)
		switch(str[i]) {
		case '&':
			strcpy(quote_buf + pos, "&amp;");
			pos += 5;
			break;
		case '<':
			strcpy(quote_buf + pos, "&lt;");
			pos += 4;
			break;
		case '>':
			strcpy(quote_buf + pos, "&gt;");
			pos += 4;
			break;
		case '"':
			strcpy(quote_buf + pos, "&quot;");
			pos += 6;
			break;
		default:
			quote_buf[pos++] = str[i];
			break;
		}

	quote_buf[pos++] = '"';
	quote_buf[pos++] = 0;
	return quote_buf;
}


void init_xml_str_attr(tag_t attr, char const * str, char * buffer)
{

	if (sprintf(tmp_buf, " %s=""%s""", xml_tag_name(attr), quote(str)) < 0) {
			fprintf(stderr,"init_xml_str_attr: sprintf failed: %s\n",
				strerror(errno));
			exit(EXIT_FAILURE);
	}
	strcat(buffer, tmp_buf);
}
