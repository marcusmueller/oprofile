/**
 * @file oprofpp.cpp
 * Main post-profiling tool
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 * 
 * @author John Levon <moz@compsoc.man.ac.uk>
 * @author Philippe Elie <phil_el@wanadoo.fr>
 */

#include <algorithm>
#include <sstream>
#include <list>
#include <fstream>

#include "version.h"
#include "oprofpp.h"
#include "oprofpp_options.h"
#include "op_libiberty.h"
#include "op_fileio.h"
#include "op_mangling.h"
#include "file_manip.h"
#include "samples_container.h"
#include "derive_files.h"

using std::string;
using std::vector;
using std::cout;
using std::cerr;
using std::ifstream;
using std::list;
using std::ostringstream;
 

/**
 * do_list_symbols - list symbol samples for an image
 * @param abfd the bfd object from where come the samples
 * @param samples_files the samples files where are stored samples
 * @param cmask on what counters we work
 * @param sort_by_ctr the counter number used for sort purpose
 *
 * Lists all the symbols in decreasing sample count order, to standard out.
 */
static void do_list_symbols(samples_container_t & samples,
			    OutputSymbol & out, int sort_by_ctr)
{
	vector<symbol_entry const *> symbols =
		samples.select_symbols(sort_by_ctr, 0.0, false);

	out.Output(cout, symbols, options::reverse_sort == false);
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
static void do_list_symbols_details(samples_container_t & samples,
				    OutputSymbol & out, int sort_by_ctr)
{
	vector<symbol_entry const *> symbols =
		samples.select_symbols(sort_by_ctr, 0.0, false, true);

	out.Output(cout, symbols, false);
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
static void do_list_symbol(samples_container_t & samples, OutputSymbol & out)
{
	symbol_entry const * symb = samples.find_symbol(options::symbol);
	if (symb == 0) {
		cerr << "oprofpp: symbol \"" << options::symbol
		     << "\" not found in samples container file.\n";
		return;
	}

	out.Output(cout, symb);
}

#define GMON_VERSION 1
#define GMON_TAG_TIME_HIST 0
#define MULTIPLIER 2

struct gmon_hdr {
	char cookie[4];
	u32 version;
	u32 spare[3];
};
 
/**
 * do_dump_gprof - produce gprof sample output
 * @param abfd the bfd object from where come the samples
 * @param samples_files the samples files where are stored samples
 * @param sort_by_ctr the counter number used for sort purpose
 *
 * Dump gprof-format samples for this sample file and
 * counter specified ctr to the file specified by gproffile.
 *
 * this use the grpof format <= gcc 3.0
 */
static void do_dump_gprof(op_bfd & abfd,
			  opp_samples_files const & samples_files,
			  int sort_by_ctr)
{
	static gmon_hdr hdr = { { 'g', 'm', 'o', 'n' }, GMON_VERSION, {0,0,0,},}; 
	u32 start, end;
	uint j;
	u32 low_pc;
	u32 high_pc;
	u16 * hist;
	u32 histsize;

	FILE * fp = op_open_file(options::gprof_file.c_str(), "w");

	op_write_file(fp,&hdr, sizeof(gmon_hdr));

	op_write_u8(fp, GMON_TAG_TIME_HIST);

	abfd.get_vma_range(low_pc, high_pc);

	// FIXME : is this (high - low - (MUL -1)) / MULT ? need a test ...
	histsize = ((high_pc - low_pc) / MULTIPLIER) + 1; 
 
	op_write_u32(fp, low_pc);
	op_write_u32(fp, high_pc);
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
			pos = (abfd.sym_offset(i, j) + abfd.syms[i].vma() - low_pc) / MULTIPLIER; 

			/* opp_get_options have set ctr to one value != -1 */
			count = samples_files.samples_count(sort_by_ctr, j);

			if (pos >= histsize) {
				cerr << "Bogus histogram bin " << pos 
				     << ", larger than " << pos << " !\n";
				continue;
			}

			if (hist[pos] + count > (u16)-1) {
				// FIXME cout or cerr ?
				cout <<	"Warning: capping sample count by "
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

// FIXME: move to util++... 
/**
 * file_exist - test for a samples file existence
 * @param filename the base samples filename
 *
 * return true if filename exist
 */
static bool file_exist(string const & filename)
{
	return ifstream(filename.c_str());
}

/**
 * main
 */
int main(int argc, char const *argv[])
{
	string const arg = get_options(argc, argv);

	int ctr_mask = counter_mask(options::ctr_str);

	derive_files(arg, options::image_file,
		options::sample_file, ctr_mask);

	validate_counter(ctr_mask, options::sort_by_counter);

	options::sample_file =
		relative_to_absolute_path(options::sample_file, OP_SAMPLES_DIR);

	if (!options::gprof_file.empty()) {
		opp_samples_files samples_files(options::sample_file, ctr_mask);
		check_mtime(samples_files, options::image_file);

		op_bfd abfd(options::image_file);
		samples_files.set_start_offset(abfd.get_start_offset());
		do_dump_gprof(abfd, samples_files, options::sort_by_counter);
		return 0;
	}

	options::output_format_flags = 
		static_cast<OutSymbFlag>(options::output_format_flags | osf_show_all_counters);
	if (!options::symbol.empty() || options::list_all_symbols_details)
		options::output_format_flags = 
			static_cast<OutSymbFlag>(options::output_format_flags | osf_details);

	/* create the file list of samples files we will use (w/o the
	 * #counter_nr suffix) */
	list<string> filelist;
	if (options::show_shared_libs) {
		string const dir = dirname(options::sample_file);
		string const name = basename(options::sample_file);
 
		get_sample_file_list(filelist, dir, name + "}}}*");
	}

	bool const add_zero_sample_symbols = options::list_all_symbols_details == false;
 
	samples_container_t samples(add_zero_sample_symbols,
				    options::output_format_flags, ctr_mask);

	filelist.push_front(options::sample_file);

	bool first_file = true;

	list<string>::const_iterator it;
	for (it = filelist.begin() ; it != filelist.end() ; ++it) {
		string const dir = dirname(options::sample_file);

		// shared libraries are added to the file list as relative paths
		// so we fix that up here
		string file = relative_to_absolute_path(*it, dir);
		 
		int i;
		for (i = 0 ; i < OP_MAX_COUNTERS ; ++i) {
			if ((ctr_mask & (1 << i)) != 0) {
				ostringstream s;
				s << file << "#" << i;
				if (file_exist(s.str()) == true)
					break;
			}
		}

		// no profiles found
		if (i == OP_MAX_COUNTERS)
			continue;

		opp_samples_files samples_files(file, ctr_mask);

		// the first opened file is treated specially because
		// user can specify the image name for this sample
		// file on command line else must deduce the image
		// name from the samples file name
		if (it == filelist.begin()) {
			check_mtime(samples_files, options::image_file);

			op_bfd abfd(options::image_file);

			samples_files.set_start_offset(abfd.get_start_offset());

			samples.add(samples_files, abfd);
		} else {
			string app_name;
			string lib_name;
			app_name = extract_app_name(file, lib_name);

			check_mtime(samples_files, options::image_file);

			op_bfd abfd(demangle_filename(lib_name));
			samples_files.set_start_offset(abfd.get_start_offset());

			samples.add(samples_files, abfd);
		}

		if (first_file == true) {
			samples_files.output_header();
			first_file = false;
		}
	}

	if (first_file == true) {
		quit_error("oprofpp: Cannot locate any samples file.\n");
	}

	OutputSymbol out(samples, ctr_mask);
	out.SetFlag(options::output_format_flags);

	if (options::list_symbols)
		do_list_symbols(samples, out, options::sort_by_counter);
	else if (!options::symbol.empty())
		do_list_symbol(samples, out);
	else if (options::list_all_symbols_details)
		do_list_symbols_details(samples, out, options::sort_by_counter);

	return 0;
}

