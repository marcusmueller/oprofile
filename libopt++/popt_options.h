/**
 * @file popt_options.h
 * option parsing
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
 * @param argc like the parameter of main()
 * @param argv like the paramter of main()
 * @param additional_param an additional optional parameter is stored here
 */
void parse_options(int argc, char const ** argv,
		   std::string & additional_parm);

/**
 * @param argc like the parameter of main()
 * @param argv like the paramter of main()
 * @param additional_param additionals optional parameters are stored here
 */
void parse_options(int argc, char const ** argv,
		   std::vector<std::string> & additional_parms);

template <class T> class option;

/** the base class for all options type FIXME more doc (example of code ?) */
class option_base {
public:
	option_base(char const * option_name, char short_name,
		    char const * help_str, char const * arg_help_str,
		    void * data, int popt_flags);
	virtual ~option_base() {}

	/* default implementation do nothing: some derived class need no work
	 * after popt argument processing */
	virtual void post_process() {}
};

/** specializaton for void type is by definition a seen / not seen option
 * like POPT_ARG_NONE */
template <> class option<void> : public option_base {
public:
	option(bool & value, char const * option_name, char short_name,
	       char const * help_str);

	void post_process();
private:
	bool & value;
	int popt_value;
};

/** POPT_ARG_INT */
template <> class option<int> : public option_base {
public:
	option(int & value, char const * option_name, char short_name,
	       char const * help_str, char const * arg_help_str);
};

/** POPT_ARG_STRING */
template <> class option<std::string> : public option_base {
public:
	option(std::string & value,char const * option_name, char short_name,
	       char const * help_str, char const * arg_help_str);
	void post_process();
private:
	// we need an intermediate string to pass it to popt libs
	char * popt_value;
	std::string & value;
};

/** no POPT_ARG_xxx equivalent */
template <> class option< std::vector<std::string> > : public option_base {
public:
	option(std::vector<std::string> & value,
	       char const * option_name, char short_name,
	       char const * help_str, char const * arg_help_str,
	       char separator = ',');
	void post_process();
private:
	std::vector<std::string> & value;
	// we need an intermediate string to pass it to popt libs
	char * popt_value;
	const char separator;
};

#endif /* ! POPT_OPTIONS_H */
