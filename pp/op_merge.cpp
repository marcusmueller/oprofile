/**
 * @file op_merge.cpp
 * Merge sample files
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 */

#include <stdio.h>

#include <vector>
#include <list>
#include <string>
#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>

#include "version.h"
#include "popt_options.h"
#include "file_manip.h"
#include "cverb.h"

#include "op_config.h"
#include "op_mangling.h"
#include "op_sample_file.h"
#include "samples_file.h"

using std::string;
using std::vector;
using std::list;
using std::replace;
using std::ostringstream;
using std::ofstream;
using std::cerr;
using std::endl;
using std::ifstream;

namespace {
	int counter;
	bool verbose;
}

option options_array[] = {
	option(verbose, "verbose", 'V', "verbose output"),
	option(counter, "use-counter", 'c', "use counter", "counter nr")
};

/**
 * get_options - process command line
 * @param argc program arg count
 * @param argv program arg array
 * @param image: where to store the images filename
 *
 * Process the arguments, fatally complaining on error.
 */
static void get_options(int argc, char const * argv[], vector<string> & images)
{
	parse_options(argc, argv, images);

	set_verbose(verbose);

	if (images.size() == 0) {
		cerr << "Neither samples filename or image filename"
		     <<	" given on command line\n\n";
	}
}

/**
 * create_file_list - create the file list based
 *  on the command line file list
 * @param result where to store the created file list
 * @param images_filename the input file list contains either a binary
 * image name or a list of samples filenames
 *
 * if images_filename contain only one file it is assumed to be
 * the name of binary image and the created file list will
 * contain all samples file name related to this image
 * else the result contains only filename explicitly
 * specified in images_filename
 *
 * all error are fatal.
 */
static void create_file_list(list<string> & result,
			     vector<string> const & images_filename)
{
	/* user can not mix binary name and samples files name on the command
	 * line, this error is captured later because when more one filename
	 * is given all files are blindly loaded as samples file and so on
	 * a fatal error occur at load time. FIXME for now I see no use to
	 * allow mixing samples filename and binary name but later if we
	 * separate samples for each this can be usefull? */
	if (images_filename.size() == 1 &&
	    images_filename[0].find_first_of('{') == string::npos) {
		/* get from the image name all samples on the form of
		 * base_dir*}}mangled_name{{{images_filename) */
		ostringstream os;
		os << "*}}" << remangle_filename(images_filename[0])
		   << "#" << counter;

		get_sample_file_list(result, OP_SAMPLES_DIR, os.str());

		// get_sample_file_list() shrink the #nr suffix so re-add it
		list<string>::iterator it;
		for (it = result.begin() ; it != result.end() ; ++it) {
			ostringstream os;
			os << string(OP_SAMPLES_DIR) << "/" << *it << "#" << counter;
			*it = os.str();
		}
	} else {
		/* Note than I don't check against filename i.e. all filename
		 * must not necessarilly belongs to the same application. We
		 * check later only for coherent opd_header. If we check
		 * against filename string we disallow the user to merge
		 * already merged samples file */

		/* That's a common pitfall through regular expression to
		 * provide more than one time the same filename on command
		 * file, just silently remove duplicate */
		for (size_t i = 0 ; i < images_filename.size(); ++i) {
			if (find(result.begin(), result.end(),
				 images_filename[i]) == result.end())
				result.push_back(images_filename[i]);
		}
	}

	if (result.size() == 0) {
		cerr << "No samples files found" << endl;
		exit(EXIT_FAILURE);
	}
}

/**
 * check_samples_files_list - chack than all samples have coherent header
 * @param filenames: the filenames list from which we check sample file
 *
 * all error are fatal
 */
static void check_samples_files_list(list<string> const & filenames)
{
	if (filenames.empty())
		return;

	samples_file_t first(*filenames.begin());

	list<string>::const_iterator it;
	for (it = filenames.begin(); ++it != filenames.end(); ) {
		samples_file_t next(*it);

		first.check_headers(next);
	}
}

/**
 * callback used to merge a database to another database.
 *
 * @param key
 * @param value
 * @param data is a pointer to the destination db_tree_t object
 */
static void copy_callback(db_key_t key, db_value_t value, void * data)
{
	db_tree_t * dest = (db_tree_t *)data;

	db_insert(dest, key, value);
}

/**
 * output_files - create a samples file by merging (cumulating samples count)
 *  from a collection of samples files
 * @param filename the output filename
 * @param filenames a collection of samples files name
 *
 * all error are fatal
 */
static void output_files(string const & filename,
			 list<string> const & filenames)
{
	if (filenames.empty())
		return;

	list<string>::const_iterator it(filenames.begin());

	// the first file merged is copied as it so opd_header is initialized
	{
		ofstream out(filename.c_str());
		ifstream in(it->c_str());

		out << in.rdbuf();
	}

	db_tree_t dest;

	db_open(&dest, filename.c_str(), DB_RDWR, sizeof(struct opd_header));

	for (++it ; it != filenames.end() ; ++it) {
		db_tree_t src;

		db_open(&src, it->c_str(), DB_RDONLY, sizeof(struct opd_header));

		db_travel(&src, 0, ~0, copy_callback, &dest);

		db_close(&src);
	}

	db_close(&dest);
}

//---------------------------------------------------------------------------

int main(int argc, char const * argv[])
{
	vector<string> images_filename;
	list<string> samples_filenames;

	get_options(argc, argv, images_filename);

	create_file_list(samples_filenames, images_filename);

	check_samples_files_list(samples_filenames);

	string libname;
	extract_app_name(*samples_filenames.begin(), libname);

	output_files(libname, samples_filenames);

	return EXIT_SUCCESS;
}

