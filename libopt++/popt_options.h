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
 * bool allow_frob;
 * string frob;
 * static option allow_frob_opt(allow_frob, "allow-frob", 'a', "allow frobs");
 * static option frob_opt(frob, "frob", 'f', "what to frob", "name");
 *
 * ...
 * parse_options(argc, argv, add_params);
 * \endcode
 *
 * Note than if you try to implement an option for an unsupported type  like :
 * \code
 * static unsigned int i;
 * static option i_opt(i, ....);
 * \endcode
 * you don't get a compile time error but a link time error.
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

class option_base;

/**
 * option_base - base class for a command line option
 *
 * Every command line option added before calling parse_options()
 * is of this type.
 */
class option {
public:
	/** we don't define a generic implementation of this ctor, we
	 * only declare/define specialization for the supported option type
	 */
	template <class T> option(T &, char const * option_name,
				  char short_name, char const * help_str,
				  char const * arg_help_str = 0);
	~option();
private:
	option_base * the_option;
};

/** The supported option type */
template <> option::option(bool &, char const * option_name, char short_name,
			   char const * help_str);
template <> option::option(int &, char const * option_name, char short_name,
			   char const * help_str, char const * arg_help_str);
template <> option::option(std::string &, char const * option_name,
			   char short_name, char const * help_str,
			   char const * arg_help_str);
template <> option::option(std::vector<std::string> &,
			   char const * option_name, char short_name,
			   char const * help_str, char const * arg_help_str);

#endif /* ! POPT_OPTIONS_H */
