/**
 * @file popt_options.cpp
 * option parsing
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie <phil_el@wanadoo.fr>
 * @author John Levon <moz@compsoc.man.ac.uk>
 */

#include <iostream>

#include "op_popt.h"
#include "version.h"

#include "popt_options.h"
#include "string_manip.h"

using std::vector;
using std::string;
using std::cerr;

/** the popt array singleton options */
static vector<poptOption> popt_options;
static vector<option_base*> options_list;

static int showvers;
static struct poptOption appended_options[] = {
  { "version", 'v', POPT_ARG_NONE, &showvers, 0, "show version", NULL, },
  POPT_AUTOHELP
  POPT_TABLEEND
  };

static poptContext do_parse_options(int argc, char const ** argv,
				    vector<string> & additional_parms)
{
	vector<poptOption> options(popt_options);

	int const nr_appended_options =
		sizeof(appended_options) / sizeof(appended_options[0]);

	options.insert(options.end(), appended_options,
		       appended_options + nr_appended_options);

	poptContext con = op_poptGetContext(NULL, argc, argv, &options[0], 0);

	if (showvers) {
		show_version(argv[0]);
	}

	char const * file;
	while ((file = poptGetArg(con)) != 0) {
		additional_parms.push_back(file);
	}

	for (size_t i = 0 ; i < options_list.size() ; ++i) {
		options_list[i]->post_process();
	}

	return con;
}

void parse_options(int argc, char const ** argv,
		   vector<string> & additional_parms)
{
	poptContext con = do_parse_options(argc, argv, additional_parms);

	poptFreeContext(con);
}

void parse_options(int argc, char const ** argv, string & additional_parm)
{
	vector<string> additional_parms;
	poptContext con = do_parse_options(argc, argv, additional_parms);

	if (additional_parms.size() > 1) {
		cerr << "too many arguments\n";
		poptPrintHelp(con, stderr, 0);
		poptFreeContext(con);
		exit(EXIT_FAILURE);
	}

	if (additional_parms.size() == 1)
		additional_parm = additional_parms[0];

	poptFreeContext(con);
}

option_base::option_base(char const * option_name, char short_name,
			 char const * help_str, char const * arg_help_str,
			 void * data, int popt_flags)
{
	poptOption opt = { option_name, short_name, popt_flags,
			     data, 0, help_str, arg_help_str};

	popt_options.push_back(opt);

	options_list.push_back(this);
}

option<void>::option(bool & value_,
		     char const * option_name, char short_name,
		     char const * help_str)
	:
	option_base(option_name, short_name, help_str, 0, &popt_value,
		    POPT_ARG_NONE),
	value(value_),
	popt_value(0)
{
}

void option<void>::post_process()
{
	value = popt_value != 0;
}

option<int>::option(int & value, char const * option_name, char short_name,
		    char const * help_str, char const * arg_help_str)
	:
	option_base(option_name, short_name, help_str, arg_help_str, &value,
		    POPT_ARG_INT)
{
}

option<string>::option(string & value_, char const * option_name,
		       char short_name, char const * help_str,
		       char const * arg_help_str)
	:
	option_base(option_name, short_name, help_str, arg_help_str,
		    &popt_value, POPT_ARG_STRING),
	popt_value(0),
	value(value_)
{
}

void option<string>::post_process()
{
	if (popt_value) {
		value = popt_value;
		popt_value = 0;
	}
}

option< vector<string> >::option(vector<string> & value_,
				 char const * option_name, char short_name,
				 char const * help_str, 
				 char const * arg_help_str,
				 char separator_)
	:
	option_base(option_name, short_name, help_str, arg_help_str,
		    &popt_value, POPT_ARG_STRING),
	value(value_),
	popt_value(0),
	separator(separator_)
{
}

void option< vector<string> >::post_process()
{
	if (popt_value) {
		separate_token(value, popt_value, separator);  

		popt_value = 0;
	}
}


