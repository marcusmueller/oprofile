/**
 * @file oprofpp.cpp
 * Main post-profiling tool
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 */

#include <algorithm>
#include <list>
#include <fstream>

#include "version.h"
#include "oprofpp_options.h"
#include "session.h"
#include "op_libiberty.h"
#include "op_fileio.h"
#include "file_manip.h"
#include "string_manip.h"

#include "op_mangling.h"
#include "profile_container.h"
#include "profile.h"
#include "counter_util.h"
#include "derive_files.h"
#include "format_output.h"

#include "op_exception.h"

using namespace std;

/**
 * do_list_symbols - list symbol samples for an image
 * @param abfd the bfd object from where come the samples
 * @param samples_files the samples files where are stored samples
 * @param cmask on what counters we work
 * @param sort_by_ctr the counter number used for sort purpose
 *
 * Lists all the symbols in decreasing sample count order, to standard out.
 */
static void do_list_symbols(profile_container_t const & samples,
			    format_output::formatter & out, int sort_by_ctr)
{
	vector<symbol_entry const *> symbols =
		samples.select_symbols(sort_by_ctr, 0.0, false);

	bool need_vma64 = vma64_p(symbols.begin(), symbols.end());
	out.output(cout, symbols, !options::reverse_sort, need_vma64);
}

/**
 * do_list_symbols_details - list all samples for all symbols.
 * @param abfd the bfd object from where come the samples
 * @param samples_files the samples files where are stored samples
 * @param cmask on what counters we work
 * @param sort_by_ctr the counter number used for sort purpose
 *
 * Lists all the samples for all the symbols, from the image specified by
 * abfd, in increasing order of vma, to standard out.
 */
static void do_list_symbols_details(profile_container_t const & samples,
				    format_output::formatter & out, int sort_by_ctr)
{
	vector<symbol_entry const *> symbols =
		samples.select_symbols(sort_by_ctr, 0.0, false, true);

	bool need_vma64 = vma64_p(symbols.begin(), symbols.end());
	out.output(cout, symbols, false, need_vma64);
}

/**
 * do_list_symbol - list detailed samples for a symbol
 * @param abfd the bfd object from where come the samples
 * @param samples_files the samples files where are stored samples
 * @param cmask on what counters we work
 *
 * the global variable symbol is used to list all
 * the samples for this symbol from the image
 * specified by abfd.
 */
static void do_list_symbol(profile_container_t const & samples, format_output::formatter & out)
{
	symbol_entry const * symb = samples.find_symbol(options::symbol);
	if (symb == 0) {
		cerr << "oprofpp: symbol \"" << options::symbol
		     << "\" not found in samples container file.\n";
		return;
	}

	bool need_vma64 = vma64_p(&symb, &symb + 1);
	out.output(cout, symb, need_vma64);
}


#define GMON_VERSION 1
#define GMON_TAG_TIME_HIST 0

struct gmon_hdr {
	char cookie[4];
	u32 version;
	u32 spare[3];
};


static void op_write_vma(FILE * fp, op_bfd const & abfd, bfd_vma vma)
{
	// bfd vma write size is a per binary property not a bfd
	// configuration property
	switch (abfd.bfd_arch_bits_per_address()) {
		case 32:
			op_write_u32(fp, vma);
			break;
		case 64:
			op_write_u64(fp, vma);
			break;
		default:
			cerr << "oprofile: unknwon vma size for this binary\n";
			exit(EXIT_FAILURE);
	}
}


/**
 * @param samples_files  profile container to act on
 * @param sort_by_ctr  the used counter number
 * @param gap  a power of 2
 *
 * return true if all sample in samples_files are at least aligned on gap. This
 * function is used to get at runtime the right size of gprof bin size
 * reducing gmon.out on arch with fixed size instruction length
 *
 */
bool aligned_samples(op_bfd const & abfd,  profile_t const & samples_files,
		     int sort_by_ctr, int gap)
{
	u32 start, end;
	uint j;
	bfd_vma low_pc;
	bfd_vma high_pc;

	// FIXME: see do_dump_gprof()
	abfd.get_vma_range(low_pc, high_pc);

	for (symbol_index_t i = 0; i < abfd.syms.size(); i++) {
		abfd.get_symbol_range(i, start, end);
		for (j = start; j < end; j++) {
			u32 count;
			u32 pos;
			// we must offset by multiplier - 1 so rounding during
			// division don't put samples in the previous chunk
			pos = (abfd.sym_offset(i, j) + abfd.syms[i].vma() - low_pc);

			/* opp_get_options have set ctr to one value != -1 */
			count = samples_files.samples_count(sort_by_ctr, j);
			if (count && (pos % gap))
				return false;
		}
	}

	return true;
}


/**
 * do_dump_gprof - produce gprof sample output
 * @param abfd the bfd object from where come the samples
 * @param samples_files the samples files where are stored samples
 * @param sort_by_ctr the counter number used for sort purpose
 *
 * Dump gprof-format samples for this sample file and
 * counter specified ctr to the file specified by gproffile.
 */
static void do_dump_gprof(op_bfd const & abfd,
			  profile_t const & samples_files,
			  int sort_by_ctr)
{
	static gmon_hdr hdr = { { 'g', 'm', 'o', 'n' }, GMON_VERSION, {0,0,0,},};
	u32 start, end;
	uint j;
	bfd_vma low_pc;
	bfd_vma high_pc;
	u16 * hist;
	u32 histsize;

	FILE * fp = op_open_file(options::gprof_file.c_str(), "w");

	int multiplier = 2;
	if (aligned_samples(abfd, samples_files, sort_by_ctr, 4))
		multiplier = 8;

	/* FIXME worth to try more multiplier ? is ia64 with its chunk of
	 * instructions can get sample inside a chunck or always at chunk
	 * boundary ? */

	op_write_file(fp,&hdr, sizeof(gmon_hdr));

	op_write_u8(fp, GMON_TAG_TIME_HIST);

	// FIXME: we sur-estimate a lot the vma range here, we can shrink a lot
	// the size of gmon.out by getting the lowest/higest vma with sample
	abfd.get_vma_range(low_pc, high_pc);

	histsize = ((high_pc - low_pc + multiplier - 1) / multiplier) + 1;

	op_write_vma(fp, abfd, low_pc);
	op_write_vma(fp, abfd, high_pc);
	/* size of histogram */
	op_write_u32(fp, histsize);
	/* profiling rate */
	op_write_u32(fp, 1);
	op_write_file(fp, "samples\0\0\0\0\0\0\0\0", 15);
	/* abbreviation */
	op_write_u8(fp, '1');

	hist = (u16*)xcalloc(histsize, sizeof(u16));

	for (symbol_index_t i = 0; i < abfd.syms.size(); i++) {
		abfd.get_symbol_range(i, start, end);
		for (j = start; j < end; j++) {
			u32 count;
			u32 pos;
			// we must offset by multiplier - 1 so rounding during
			// division don't put samples in the previous chunk
			pos = (abfd.sym_offset(i, j) + abfd.syms[i].vma() - low_pc + multiplier - 1) / multiplier;

			/* opp_get_options have set ctr to one value != -1 */
			count = samples_files.samples_count(sort_by_ctr, j);

			if (pos >= histsize) {
				cerr << "Bogus histogram bin " << pos
				     << ", larger than " << pos << " !\n";
				continue;
			}

			if (hist[pos] + count > (u16)-1) {
				cerr <<	"Warning: capping sample count by "
				     << hist[pos] + count - ((u16)-1)
				     << "samples for symbol \""
				     << abfd.syms[i].name() << "\"\n";
			} else {
				hist[pos] += (u16)count;
			}
		}
	}

	op_write_file(fp, hist, histsize * sizeof(u16));
	op_close_file(fp);

	free(hist);
}


static int do_oprofpp(int argc, char const * argv[])
{
	string const arg = get_options(argc, argv);

	derive_files(arg, options::image_file,
		options::sample_file, options::counter_mask);

	validate_counter(options::counter_mask, options::sort_by_counter);

	string samples_dir = handle_session_options();

	options::sample_file =
		relative_to_absolute_path(options::sample_file, samples_dir);

	if (!options::gprof_file.empty()) {
		profile_t profile(options::sample_file, options::counter_mask);
		profile.check_mtime(options::image_file);

		op_bfd abfd(options::image_file, options::exclude_symbols,
			    vector<string>());
		profile.set_start_offset(abfd.get_start_offset());
		do_dump_gprof(abfd, profile, options::sort_by_counter);
		return 0;
	}

	if (!options::symbol.empty() || options::list_all_symbols_details)
		options::output_format_flags =
			static_cast<outsymbflag>(options::output_format_flags | osf_details);

	/* create the file list of samples files we will use (w/o the
	 * #counter_nr suffix) */
	list<string> filelist;
	if (options::show_shared_libs) {
		string const dir = dirname(options::sample_file);
		string const name = basename(options::sample_file);

		get_sample_file_list(filelist, dir, name + "}}}*");
	}

	profile_container_t samples(!options::list_all_symbols_details,
				    options::output_format_flags, options::counter_mask);

	filelist.push_front(options::sample_file);

	bool found_file = false;

	list<string>::const_iterator it;
	for (it = filelist.begin() ; it != filelist.end() ; ++it) {
		string const dir = dirname(options::sample_file);

		// shared libraries are added to the file list as relative
		// paths so we fix that up here
		string file = relative_to_absolute_path(*it, dir);

		int i;
		for (i = 0 ; i < OP_MAX_COUNTERS ; ++i) {
			if ((options::counter_mask & (1 << i)) != 0) {
				string filename = 
					sample_filename(string(), file, i);
				if (op_file_readable(filename))
					break;
			}
		}

		// no profiles found
		if (i == OP_MAX_COUNTERS)
			continue;

		profile_t profile(file, options::counter_mask);

		// the first opened file is treated specially because user can
		// specify the image name for this sample file on command line
		// we must deduce the image name from the samples file name
		if (it == filelist.begin()) {
			profile_t profile(file, options::counter_mask);

			string image_name =
			  check_image_name(options::alternate_filename,
					   options::image_file, file);
			if (image_name.empty()) {
				// check_image_name() already output a warning
				// we exit() because we can't accept the
				// primary image can't be found
				exit(EXIT_FAILURE);
			}

			op_bfd abfd(image_name,
				    options::exclude_symbols,
				    vector<string>());

			profile.check_mtime(image_name);
			profile.set_start_offset(abfd.get_start_offset());
			samples.add(profile, abfd, image_name,
				    options::symbol);

			profile.output_header();
		} else {
			string app_name;
			string lib_name;
			app_name = extract_app_name(file, lib_name);
			app_name = demangle_filename(app_name);
			app_name =
				check_image_name(options::alternate_filename,
						 app_name, file);
			if (app_name.empty()) {
				// we can't accept app_name is not found (imho)
				exit(EXIT_FAILURE);
			}
			lib_name = demangle_filename(lib_name);
			lib_name =
				check_image_name(options::alternate_filename,
						 lib_name, file);
			if (lib_name.empty()) {
				// user is already warned this file has been
				// ignored, we continue in case this is a
				// spurious error e.g. user didn't specify
				// the right -p/-P options
				continue;
			}
			add_samples(samples, file, options::counter_mask,
			  lib_name, app_name, options::exclude_symbols,
				    options::symbol);
		}
		found_file = true;
	}

	if (!found_file) {
		cerr << "oprofpp: Cannot locate any samples file." << endl;
		exit(EXIT_FAILURE);
	}

	format_output::formatter out(samples, options::counter_mask);
	out.add_format(options::output_format_flags);

	if (options::list_symbols)
		do_list_symbols(samples, out, options::sort_by_counter);
	else if (!options::symbol.empty())
		do_list_symbol(samples, out);
	else if (options::list_all_symbols_details)
		do_list_symbols_details(samples, out, options::sort_by_counter);

	return 0;
}

/**
 * main
 */
int main(int argc, char const * argv[])
{
	try {
		return do_oprofpp(argc, argv);
	}
	catch (op_runtime_error const & e) {
		cerr << "op_runtime_error:" << e.what() << endl;
		return 1;
	}
	catch (op_fatal_error const & e) {
		cerr << "op_fatal_error:" << e.what() << endl;
	}
	catch (op_exception const & e) {
		cerr << "op_exception:" << e.what() << endl;
	}
	catch (exception const & e) {
		cerr << "exception:" << e.what() << endl;
	}
	catch (...) {
		cerr << "unknown exception" << endl;
	}

	return 0;
}
