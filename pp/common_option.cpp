/**
 * @file common_option.cpp
 * Contains common options and implementation of entry point of pp tools
 * and some miscelleaneous functions
 *
 * @remark Copyright 2003 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 */

#include <iostream>
#include <sstream>

#include "op_exception.h"
#include "popt_options.h"
#include "cverb.h"
#include "common_option.h"
#include "file_manip.h"

using namespace std;

namespace options {
	bool verbose;
	extra_images extra_found_images;
	double threshold = 0.0;
}

namespace {

string threshold;
vector<string> image_path;

popt::option options_array[] = {
	popt::option(options::verbose, "verbose", 'V',
		     "verbose output"),
	popt::option(image_path, "image-path", 'p',
		     "comma-separated path to search missing binaries","path"),
	popt::option(threshold, "threshold", 't',
		     "minimum percentage needed to produce output",
		     "percent"),
};


double handle_threshold(string threshold)
{
	double value = 0.0;

	if (threshold.length()) {
		istringstream ss(threshold);
		if (!(ss >> value)) {
			cerr << "illegal threshold value: " << threshold
			     << " allowed range: [0-100]" << endl;
			exit(EXIT_FAILURE);
		}

		if (value < 0.0 || value > 100.0) {
			cerr << "illegal threshold value: " << threshold
			     << " allowed range: [0-100]" << endl;
			exit(EXIT_FAILURE);
		}
	}

	cverb << value << endl;;

	return value;
}


vector<string> get_options(int argc, char const * argv[])
{
	vector<string> non_options;
	popt::parse_options(argc, argv, non_options);

	if (!::threshold.empty())
		options::threshold = handle_threshold(::threshold);

	set_verbose(options::verbose);

	bool ok = true;
	vector<string>::const_iterator it;
	for (it = image_path.begin(); it != image_path.end(); ++it) {
		if (!is_directory(*it)) {
			cerr << *it << " isn't a valid directory\n";
			ok = false;
		}
	}

	if (!ok)
		throw op_runtime_error("invalid --image-path= options");

	options::extra_found_images.populate(image_path);

	return non_options;
}

}


int run_pp_tool(int argc, char const * argv[], pp_fct_run_t fct)
{
	try {
		vector<string> non_options = get_options(argc, argv);

		return fct(non_options);
	}
	catch (op_runtime_error const & e) {
		cerr << argv[0] << " error: " << e.what() << endl;
	}
	catch (op_fatal_error const & e) {
		cerr << argv[0] << " error: " << e.what() << endl;
	}
	catch (op_exception const & e) {
		cerr << argv[0] << " error: " << e.what() << endl;
	}
	catch (invalid_argument const & e) {
		cerr << argv[0] << " error: " << e.what() << endl;
	}
	catch (exception const & e) {
		cerr << argv[0] << " error: " << e.what() << endl;
	}
	catch (...) {
		cerr << argv[0] << " unknown exception" << endl;
	}

	return EXIT_FAILURE;
}
