/**
 * @file popt_options.h
 * option parsing
 *
 * This provides a simple facility for adding command-line
 * options, and parsing them.
 *
 * You can add a number of options and then call parse_options()
 * to process them, for example :
 *
 * \code
 *
 * bool allow_frob;
 * string frob;
 * static option<void> allow_frob_opt(allow_frob, "allow-frob", 'a', "allow frobs");
 * static option<string> frob_opt(frob, "frob", 'f', "what to frob", "name");
 *
 * ...
 * parse_options(argc, argv, add_params);
 *
 * \endcode
 *
 * The call to parse_options() will fill in allow_frob and frob, if they
 * are passed to the program (myfrobber --allow-frob --frob foo), and place
 * any left over command line arguments in the add_params vector. Note
 * that the template parameter denotes the type of the option argument.
 * 
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie <phil_el@wanadoo.fr>
 * @author John Levon <moz@compsoc.man.ac.uk>
 */

#ifndef POPT_OPTIONS_H
#define POPT_OPTIONS_H

#include <string>
#include <vector>

/**
 * parse_options - parse command line options
 * @param argc like the parameter of main()
 * @param argv like the parameter of main()
 * @param additional_param an additional option is stored here
 *
 * Parse the given command line with the previous
 * options created. If more than one normal argument
 * is given, quit with a usage message.
 */
void parse_options(int argc, char const ** argv,
		   std::string & additional_param);

/**
 * parse_options - parse command line options
 * @param argc like the parameter of main()
 * @param argv like the parameter of main()
 * @param additional_params additional options are stored here
 *
 * Parse the given command line with the previous
 * options created. Multiple additional arguments
 * that are not recognised will be added to the @additional_params
 * vector.
 */
void parse_options(int argc, char const ** argv,
		   std::vector<std::string> & additional_params);

template <class T> class option;

/**
 * option_base - base class for a command line option
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

/**
 * option<void> - a binary option
 *
 * Use this option type for constructing specified / not-specified
 * options e.g. --frob
 */
template <> class option<void> : public option_base {
public:
	option(bool & value, char const * option_name, char short_name,
	       char const * help_str);

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
template <> class option<int> : public option_base {
public:
	option(int & value, char const * option_name, char short_name,
	       char const * help_str, char const * arg_help_str);
};

/**
 * option<std::string> - a string option
 *
 * Use this for options taking a string e.g. --frob parsley
 */
template <> class option<std::string> : public option_base {
public:
	option(std::string & value,char const * option_name, char short_name,
	       char const * help_str, char const * arg_help_str);
	void post_process();
private:
	// we need an intermediate char array to pass to popt libs
	char * popt_value;
	std::string & value;
};

/**
 * option< std::vector<std::string> > - a string vector option
 *
 * Use this for options taking a number of string arguments,
 * separated by the given separator.
 */
template <> class option< std::vector<std::string> > : public option_base {
public:
	option(std::vector<std::string> & value,
	       char const * option_name, char short_name,
	       char const * help_str, char const * arg_help_str,
	       char separator = ',');
	void post_process();
private:
	std::vector<std::string> & value;
	// we need an intermediate char array to pass to popt libs
	char * popt_value;
	const char separator;
};

#endif /* ! POPT_OPTIONS_H */
