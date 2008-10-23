/**
 * @file xml_output.cpp
 * utility routines for writing XML
 *
 * @remark Copyright 2006 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Dave Nomura
 */

#include <sstream>
#include <iostream>

#include "op_xml_out.h"
#include "xml_output.h"

using namespace std;

#define MAX_XML_BUF 1024

string tag_name(tag_t tag)
{
	ostringstream out;
	out << xml_tag_name(tag);
	return out.str();
}


string open_element(tag_t tag, bool with_attrs)
{
	ostringstream out;
	char buf[MAX_XML_BUF];

	buf[0] = '\0';
	open_xml_element(tag, with_attrs, buf);
	out << buf;
	return out.str();
}


string close_element(tag_t tag, bool has_nested)
{
	ostringstream out;
	char buf[MAX_XML_BUF];

	buf[0] = '\0';
	close_xml_element(tag, has_nested, buf);
	out << buf;
	return out.str();
}


string init_attr(tag_t attr, size_t value)
{
	ostringstream out;
	char buf[MAX_XML_BUF];

	buf[0] = '\0';
	init_xml_int_attr(attr, value, buf);
	out << buf;
	return out.str();
}


string init_attr(tag_t attr, double value)
{
	ostringstream out;
	char buf[MAX_XML_BUF];

	buf[0] = '\0';
	init_xml_dbl_attr(attr, value, buf);
	out << buf;
	return out.str();
}


string init_attr(tag_t attr, string const & str)
{
	ostringstream out;
	char buf[MAX_XML_BUF];

	buf[0] = '\0';
	init_xml_str_attr(attr, str.c_str(), buf);
	out << buf;
	return out.str();
}
