/**
 * @file popt_options.cpp
 * option parsing
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 * @author John Levon
 */

#include <iostream>

#include "op_popt.h"
#include "version.h"

#include "popt_options.h"
#include "string_manip.h"

using namespace std;

/**
 * option_base - base class for implementation of a command line option
 *
 * Every command line option added before calling parse_options()
 * is of this type.
 */
class option_base {
public:
	/**
	 * option_base - construct an option with the given options.
	 * @param option_name name part of long form e.g. --option
	 * @param short_name short form name e.g. -o
	 * @param help_str short description of the option
	 * @param arg_help_str short description of the argument (if any)
	 * @param data a pointer to the data to fill in
	 * @param popt_flags the popt library data type
	 */
	option_base(char const * option_name, char short_name,
		    char const * help_str, char const * arg_help_str,
		    void * data, int popt_flags);
	virtual ~option_base() {}

	/**
	 * post_process - perform any necessary post-processing
	 */
	virtual void post_process() {}
};

/** the popt array singleton options */
static vector<poptOption> popt_options;
static vector<option_base *> options_list;

static int showvers;
static struct poptOption appended_options[] = {
  { "version", 'v', POPT_ARG_NONE, &showvers, 0, "show version", NULL, },
  POPT_AUTOHELP
  POPT_TABLEEND
  };

/* options parameter can't be a local variable because caller can use the
 * returned poptContext which contains  pointer inside the options array */
static poptContext do_parse_options(int argc, char const ** argv,
				    vector<poptOption>& options,
				    vector<string> & additional_params)
{
	options = popt_options;

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
		additional_params.push_back(file);
	}

	for (size_t i = 0 ; i < options_list.size() ; ++i) {
		options_list[i]->post_process();
	}

	return con;
}

void parse_options(int argc, char const ** argv,
		   vector<string> & additional_params)
{
	vector<poptOption> options;

	poptContext con = do_parse_options(argc, argv, options,
					   additional_params);

	poptFreeContext(con);
}

void parse_options(int argc, char const ** argv, string & additional_param)
{
	vector<poptOption> options;
	vector<string> additional_params;
	poptContext con = do_parse_options(argc, argv, options,
					   additional_params);

	if (additional_params.size() > 1) {
		cerr << "too many arguments\n";
		poptPrintHelp(con, stderr, 0);
		poptFreeContext(con);
		exit(EXIT_FAILURE);
	}

	if (additional_params.size() == 1)
		additional_param = additional_params[0];

	poptFreeContext(con);
}

template <class T> class option_imp;

/**
 * option<void> - a binary option
 *
 * Use this option type for constructing specified / not-specified
 * options e.g. --frob
 */
template <> class option_imp<void> : public option_base {
public:
	option_imp(bool & value, char const * option_name, char short_name,
	       char const * help_str);
	~option_imp() {}

	void post_process();
private:
	bool & value;
	int popt_value;
};

/**
 * option<int> - a integer option
 *
 * Use this for options taking an integer e.g. --frob 6
 */
template <> class option_imp<int> : public option_base {
public:
	option_imp(int & value, char const * option_name, char short_name,
	       char const * help_str, char const * arg_help_str);
	~option_imp() {}
};

/**
 * option<string> - a string option
 *
 * Use this for options taking a string e.g. --frob parsley
 */
template <> class option_imp<string> : public option_base {
public:
	option_imp(string & value,char const * option_name,
		   char short_name, char const * help_str,
		   char const * arg_help_str);
	void post_process();
	~option_imp() {}
private:
	// we need an intermediate char array to pass to popt libs
	char * popt_value;
	string & value;
};

/**
 * option< vector<string> > - a string vector option
 *
 * Use this for options taking a number of string arguments,
 * separated by the given separator.
 */
template <> class option_imp< vector<string> > : public option_base {
public:
	option_imp(vector<string> & value,
		   char const * option_name, char short_name,
		   char const * help_str, char const * arg_help_str,
		   char separator = ',');
	void post_process();
	~option_imp() {}
private:
	vector<string> & value;
	// we need an intermediate char array to pass to popt libs
	char * popt_value;
	char const separator;
};

option::~option()
{
	delete the_option;
}

/** specialization of option ctor for boolean option */
template <>
option::option(bool & value, char const * option_name, char short_name,
	       char const * help_str)
	: the_option(new option_imp<void>(value, option_name, short_name,
					  help_str))
{
}

/** specialization of option ctor for integer option */
template <>
option::option(int & value, char const * option_name,
	       char short_name, char const * help_str,
	       char const * arg_help_str)
	: the_option(new option_imp<int>(value, option_name, short_name,
					  help_str, arg_help_str))
{
}

/** specialization of option ctor for string option */
template <>
option::option(string & value, char const * option_name,
	       char short_name, char const * help_str,
	       char const * arg_help_str)
	: the_option(new option_imp<string>(value, option_name, short_name,
					  help_str, arg_help_str))
{
}

/** specialization of option ctor for vector<string> option */
template <>
option::option(vector<string> & value,
	       char const * option_name, char short_name,
	       char const * help_str, char const * arg_help_str)
	: the_option(new option_imp< vector<string> >(value, option_name,
						      short_name, help_str,
						      arg_help_str))
{
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

option_imp<void>::option_imp(bool & value_,
			     char const * option_name, char short_name,
			     char const * help_str)
	:
	option_base(option_name, short_name, help_str, 0, &popt_value,
		    POPT_ARG_NONE),
	value(value_),
	popt_value(0)
{
}

void option_imp<void>::post_process()
{
	value = popt_value != 0;
}

option_imp<int>::option_imp(int & value, char const * option_name,
			    char short_name, char const * help_str,
			    char const * arg_help_str)
	:
	option_base(option_name, short_name, help_str, arg_help_str, &value,
		    POPT_ARG_INT)
{
}

option_imp<string>::option_imp(string & value_, char const * option_name,
			       char short_name, char const * help_str,
			       char const * arg_help_str)
	:
	option_base(option_name, short_name, help_str, arg_help_str,
		    &popt_value, POPT_ARG_STRING),
	popt_value(0),
	value(value_)
{
}

void option_imp<string>::post_process()
{
	if (popt_value) {
		value = popt_value;
		popt_value = 0;
	}
}

option_imp< vector<string> >::option_imp(vector<string> & value_,
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

void option_imp< vector<string> >::post_process()
{
	if (popt_value) {
		separate_token(value, popt_value, separator);

		popt_value = 0;
	}
}
