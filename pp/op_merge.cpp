/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * re-written by P.Elie based on (FIXME Dave Jones ?) first implementation,
 * many cleanup by John Levon
 */

#include <stdio.h>

#include <vector>
#include <list>
#include <string>
#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>

#include "../version.h"
#include "../util/op_popt.h"
#include "../util/file_manip.h"
#include "../util/shared_ptr.h"
#include "oprofpp.h"

using std::string;
using std::vector;
using std::list;
using std::replace;
using std::ostringstream;
using std::ofstream;
using std::cerr;
using std::endl;

static int showvers;
static int counter;
static const char * base_dir;

static struct poptOption options[] = {
	{ "version", 'v', POPT_ARG_NONE, &showvers, 0, "show version", NULL, },
	{ "use-counter", 'c', POPT_ARG_INT, &counter, 0, "use counter", "counter nr", },
	{ "base-dir", 'b', POPT_ARG_STRING, &base_dir, 0, "base directory of profile daemon", NULL, },
	POPT_AUTOHELP
	{ NULL, 0, 0, NULL, 0, NULL, NULL, },
};

/**
 * get_options - process command line
 * @argc: program arg count
 * @argv: program arg array
 * @images: where to store the images filename
 *
 * Process the arguments, fatally complaining on error.
 */
static void get_options(int argc, char const * argv[], vector<string> & images)
{
	poptContext optcon;

	optcon = opd_poptGetContext(NULL, argc, argv, options, 0);

	if (showvers) {
		show_version(argv[0]);
	}

	const char * file;
	while ((file = poptGetArg(optcon)) != 0) {
		images.push_back(file);
	}

	if (images.size() == 0) {
		quit_error(optcon, "Neither samples filename or image filename"
			   " given on command line\n\n");
	}

	if (base_dir == 0)
		base_dir = "/var/opd/samples";

	poptFreeContext(optcon);
}

/**
 * That's the third version of that :/
 **/
static string mangle_filename(const string & filename)
{
	string result = filename;

	replace(result.begin(), result.end(), '/', '}');

	return result;
}

/**
 * create_file_list - create the file list based
 *  on the command line file list
 * @result: where to store the created file list
 * @images_filename: the input file list contains either a binary
 * image name or a list of samples filenames
 *
 * if @images_filename contain only one file it is assumed to be
 * the name of binary image and the created file list will
 * contain all samples file name related to this image
 * else the result contains only filename explicitly
 * specified in @images_filename
 *
 * all error are fatal.
 */
static void create_file_list(list<string> & result,
			     const vector<string> & images_filename)
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
		os << "*}}" << mangle_filename(images_filename[0])
		   << "#" << counter;

		get_sample_file_list(result, base_dir, os.str());

		// get_sample_file_list() shrink the #nr suffix so re-add it
		list<string>::iterator it;
		for (it = result.begin() ; it != result.end() ; ++it) {
			ostringstream os;
			os << string(base_dir) << "/" << *it << "#" << counter;
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
			if (find(result.begin(), result.end(), images_filename[i]) == result.end())
				result.push_back(images_filename[i]);
		}
	}

	if (result.size() == 0) {
		cerr << "No samples files found" << endl;
		exit(EXIT_FAILURE);
	}
}

/**
 * create_samples_files_list - create a collection of opened samples files
 * @filenames: the filenames list from which we open samples files
 *
 * from a collection of filenameswe create a collection of opened
 * samples files
 *
 * all error are fatal
 */ 
static void
create_samples_files_list(vector< SharedPtr<samples_file_t> > & samples_files,
			  const list<string> & filenames)
{
	list<string>::const_iterator it;
	for (it = filenames.begin(); it != filenames.end(); ++it) {
		SharedPtr<samples_file_t> p(new samples_file_t(*it));
		samples_files.push_back(p);
	}

	/* check than header are coherent */
	for (size_t i = 1 ; i < samples_files.size() ; ++i) {
		samples_files[0]->check_headers(*samples_files[i]);
	}
}

/**
 * output_files - create a samples file by merging (cumulating samples count)
 *  from a collection of samples files
 * @filename: the output filename
 * @samples_files: a collection of opened samples files
 *
 * all error are fatal
 */
static void output_files(const std::string & filename,
			 const vector< SharedPtr<samples_file_t> > & samples_files)
{
	// TODO: bad approch, we don't create a sparsed file here :/
	ofstream out(filename.c_str());

	// reinterpret's required by gcc 3, that's the standard :/
	out.write(reinterpret_cast<char*>(samples_files[0]->header), sizeof(opd_header));

	// All size of samples has been checked and must be identical
	size_t nr_samples = samples_files[0]->nr_samples;
	for (size_t i = 0 ; i < nr_samples ; ++i) {
		u32 count = 0;
		for (size_t j = 0 ; j < samples_files.size() ; ++j)
			count += samples_files[j]->samples[i].count;
		out.write(reinterpret_cast<char*>(&count), sizeof(count));
	}
}

//---------------------------------------------------------------------------

int main(int argc, char const * argv[])
{
	vector<string> images_filename;
	list<string> samples_filenames;

	get_options(argc, argv, images_filename);

	create_file_list(samples_filenames, images_filename);

	vector< SharedPtr<samples_file_t> > samples_files;
	create_samples_files_list(samples_files, samples_filenames);

	string libname;
	extract_app_name(*samples_filenames.begin(), libname);

	output_files(libname, samples_files);

	return EXIT_SUCCESS;
}

