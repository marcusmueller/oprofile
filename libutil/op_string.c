/**
 * @file op_string.c
 * general purpose C string handling implementation.
 *
 * @remark Copyright 2003 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 */

#include <string.h>
#include "op_libiberty.h"


char * op_xstrndup(char const * s, size_t len)
{
	return xmemdup(s, len, len + 1);
}


int strisprefix(char const * str, char const * prefix)
{
	return strstr(str, prefix) == str;
}


char const * skip_ws(char const * c)
{
	while (*c == ' ' || *c == '\t' || *c == '\n')
		++c;
	return c;
}


char const * skip_nonws(char const * c)
{
	while (*c && *c != ' ' && *c != '\t' && *c != '\n')
		++c;
	return c;
}


int empty_line(char const * c)
{
	return !*skip_ws(c);
}


int comment_line(char const * c)
{
	return *skip_ws(c) == '#';
}
