/* COPYRIGHT (C) 2001 Philippe Elie
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
 * first written by P.Elie, many cleanup by John Levon
 */

#include <stdio.h>

#include <vector>
#include <list>
#include <string>
#include <sstream>
#include <fstream>
#include <algorithm>


#include "../version.h"
#include "../util/op_popt.h"
#include "../util/file_manip.h"
#include "../util/shared_ptr.h"
#include "oprofpp.h"

// TODO: this is likely to become the new representation of opp_samples_files
// as: SharedPtr<samples_file> samples[OP_MAX_COUNTERS]; see later comment
struct samples_file
{
	samples_file(const string & filename, bool can_fail);
	~samples_file();

	bool check_headers(const samples_file & headers) const;

	// probably needs to be private and create the neccessary member
	// function (not simple getter), make private and compile to see
	// what operation we need later. I've currently not a clear view
	// of what we need
//private:
	opd_fentry *samples;		// header + sizeof(header)
	opd_header *header;		// mapping begin here
	fd_t fd;
	// This do not include the header size
	size_t size;

private:
	// neither copy-able or copy constructible
	samples_file(const samples_file &);
	samples_file& operator=(const samples_file &);
};

/* that's near opp_samples_files::open_samples_file() but the class
 * opp_samples_files needs to be redesigned before using it TODO:
 * clean up this etc... phe, I take care of that flame me if I forget to
 * update oprofpp in a few weeks (2002/02/25) */
samples_file::samples_file(const string & filename, bool can_fail)
	:
	samples(0),
	header(0),
	fd(-1),
	size(0)
{
	fd = open(filename.c_str(), O_RDONLY);
	if (fd == -1) {
		if (can_fail)
			return;

		fprintf(stderr, "op_merge: Opening %s failed. %s\n", filename.c_str(), strerror(errno));
		exit(EXIT_FAILURE);
	}

	size_t sz_file = opd_get_fsize(filename.c_str(), 1);
	if (sz_file < sizeof(opd_header)) {
		fprintf(stderr, "op_merge: sample file %s is not the right "
			"size: got %d, expect at least %d\n", 
			filename.c_str(), sz_file, sizeof(opd_header));
		exit(EXIT_FAILURE);
	}
	size = sz_file - sizeof(opd_header); 

	header = (opd_header*)mmap(0, sz_file, 
				   PROT_READ, MAP_PRIVATE, fd, 0);
	if (header == (void *)-1) {
		fprintf(stderr, "op_merge: mmap of %s failed. %s\n", filename.c_str(), strerror(errno));
		exit(EXIT_FAILURE);
	}

	samples = (opd_fentry *)(header + 1);

	if (memcmp(header->magic, OPD_MAGIC, sizeof(header->magic))) {
		/* FIXME: is 4.4 ok : there is no zero terminator */
		fprintf(stderr, "op_merge: wrong magic %4.4s, expected %s.\n", header->magic, OPD_MAGIC);
		exit(EXIT_FAILURE);
	}

	if (header->version != OPD_VERSION) {
		fprintf(stderr, "op_merge: wrong version 0x%x, expected 0x%x.\n", header->version, OPD_VERSION);
		exit(EXIT_FAILURE);
	}
}

samples_file::~samples_file()
{
	if (header)
		munmap(header, size + sizeof(opd_header));
	if (fd != -1)
		close(fd);
}

/* probably needs a can fail parameters */
bool samples_file::check_headers(const samples_file & rhs) const
{
	// the oprofpp free fun check_headers needs to be implemented here
	// when this class will use for opp_samples_files for his
	// implementation
	::check_headers(header, rhs.header);
	
	if (size != rhs.size) {
		fprintf(stderr, "op_merge: mapping file size "
			"are different (%d, %d)\n", size, rhs.size);
		exit(EXIT_FAILURE);		
	}

	return true;
}

/* That the real start of op_merge.cpp, garbage below must be put elsewhere */

using std::string;
using std::vector;
using std::list;
using std::replace;
using std::ostringstream;

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
		printf("op_merge: %s : " VERSION_STRING " compiled on " __DATE__ " " __TIME__ "\n", argv[0]);
		exit(EXIT_SUCCESS);
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

static void create_file_list(list<string> & result,
			     const vector<string> & images_name)
{
	/* TODO protect again people that put on cmd line binary name and
	 * samples files name, protect also against samples files specified
	 * more than once */
	if (images_name.size() == 1 && 
	    images_name[0].find_first_of('{') == string::npos) { /*}*/
		/* get from the image name all samples on the form of
		 * base_dir*}}mangled_name{{{images_name) */
		ostringstream os;
		os << "*}}" << mangle_filename(images_name[0])
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
		/* no templatized insert ? */
		result = list<string>(images_name.begin(), images_name.end());
	}
}

static void
create_samples_files_list(vector< SharedPtr<samples_file> > & samples_files,
			  const list<string> & filenames)
{
	list<string>::const_iterator it;
	for (it = filenames.begin(); it != filenames.end(); ++it) {
		SharedPtr<samples_file> p(new samples_file(*it, false));
		samples_files.push_back(p);
	}

	/* check than header are coherent */
	for (size_t i = 1 ; i < samples_files.size() ; ++i) {
		samples_files[0]->check_headers(*samples_files[i]);
	}
}

static void output_files(const std::string & filename,
			 const vector< SharedPtr<samples_file> > & samples_files)
{
	// TODO: bad approch, we don't create a sparsed file here :/
	ofstream out(filename.c_str());

	out.write(samples_files[0]->header, sizeof(opd_header));

	size_t nr_samples = samples_files[0]->size / sizeof(opd_fentry);
	for (size_t i = 0 ; i < nr_samples ; ++i) {
		u32 count = 0;
		for (size_t j = 0 ; j < samples_files.size() ; ++j)
			count += samples_files[j]->samples[i].count;
		out.write(&count, sizeof(count));
	}

	cout << filename << endl;
}

//---------------------------------------------------------------------------

int main(int argc, char const * argv[])
{
	vector<string> images_name;
	list<string> samples_filenames;

	get_options(argc, argv, images_name);

	create_file_list(samples_filenames, images_name);

	vector< SharedPtr<samples_file> > samples_files;
	create_samples_files_list(samples_files, samples_filenames);

	string libname;
	extract_app_name(*samples_filenames.begin(), libname);

	output_files(libname, samples_files);

	return EXIT_SUCCESS;
}

