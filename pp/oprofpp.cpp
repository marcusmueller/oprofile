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
#include "popt_options.h"
#include "op_libiberty.h"
#include "op_fileio.h"
#include "file_manip.h"
#include "samples_container.h"

using std::string;
using std::vector;
using std::cout;
using std::cerr;
using std::ifstream;
using std::list;
 
static string ctr_str;
static int sort_by_counter = -1;
static string gproffile;
static string symbol;
static bool list_symbols;
static bool output_linenr_info;
static bool reverse_sort;
static bool show_shared_libs;
static string output_format;
static bool list_all_symbols_details;

static OutSymbFlag output_format_flags;

static option<string> samplefile_opt(samplefile, "samples-file", 'f', "image sample file", "file");
static option<string> imagefile_opt(imagefile, "image-file", 'i', "image file", "file");
static option<void> list_symbols_opt(list_symbols, "list-symbols", 'l', "list samples by symbol");
static option<string> gproffile_opt(gproffile, "dump-gprof-file", 'g', "dump gprof format file", "file");
static option<string> symbol_opt(symbol, "list-symbol", 's', "give detailed samples for a symbol", "symbol");
static option<void> demangle_opt(demangle, "demangle", 'd', "demangle GNU C++ symbol names");
static option<string> ctr_str_opt(ctr_str, "counter", 'c', "which counter to display", "counter number[,counter nr]");
static option<int> sort_by_counter_opt(sort_by_counter, "sort", 'C', "which counter to use for sampels sort", "counter nr");
static option<void> verbose_opt(verbose, "verbose", 'V', "verbose output");
static option<void> list_all_symbols_details_opt(list_all_symbols_details, "list-all-symbols-details", 'L', "list samples for all symbols");
static option<void> output_linenr_info_opt(output_linenr_info, "output-linenr-info", 'o', "output filename:linenr info");
static option<void> reverse_sort_opt(reverse_sort, "reverse", 'r', "reverse sort order");
static option< vector<string> > exclude_symbols_opt(exclude_symbols, "exclude-symbol", 'e', "exclude these comma separated symbols", "symbol_name");
static option<void> show_shared_libs_opt(show_shared_libs, "show-shared-libs", 'k', "show details for shared libs. Only meaningfull if you have profiled with --separate-samples");
static option<string> output_format_opt(output_format, "output-format", 't', "choose the output format", "output-format strings");

/**
 * get_options - process command line
 * @param argc program arg count
 * @param argv program arg array
 * @param image_file where to store the image filename
 * @param sample_file ditto for sample filename
 * @param counter where to put the counter command line argument
 *
 * Process the arguments, fatally complaining on
 * error. Only a part of arguments analysing is
 * made here. The filename checking is deferred to
 * opp_treat_options().
 *
 */
static void opp_get_options(int argc, const char **argv, string & image_file,
			    string & sample_file, int & counter)
{
	/* non-option file, either a sample or binary image file */
	string file;
	
	parse_options(argc, argv, file);

	if (!list_all_symbols_details && !list_symbols && 
	    gproffile.empty() && symbol.empty())
		quit_error("oprofpp: no mode specified. What do you want from me ?\n");

	/* check only one major mode specified */
	if ((list_all_symbols_details + list_symbols + !gproffile.empty() + !symbol.empty()) > 1)
		quit_error("oprofpp: must specify only one output type.\n");

	if (output_linenr_info && !list_all_symbols_details && symbol.empty() && !list_symbols)
		quit_error("oprofpp: cannot list debug info without -L, -l or -s option.\n");

	if (show_shared_libs && (!symbol.empty() || !gproffile.empty())) {
		quit_error("oprofpp: you cannot specify --show-shared-libs with --dump-gprof-file or --list-symbol output type.\n");
	}
 
	if (reverse_sort && !list_symbols)
		quit_error("oprofpp: reverse sort can only be used with -l option.\n");
 
	if (show_shared_libs)
		output_format_flags = static_cast<OutSymbFlag>(output_format_flags | osf_image_name);
	if (output_linenr_info)
		output_format_flags = static_cast<OutSymbFlag>(output_format_flags | osf_linenr_info);

	if (output_format.empty()) {
		output_format = "hvspn";
	} else {
		if (!list_symbols && !list_all_symbols_details && symbol.empty()) {
			quit_error("oprofpp: --output-format can be used only without --list-symbols or --list-all-symbols-details.\n");
		}
	}

	if (list_symbols || list_all_symbols_details || !symbol.empty()) {
		OutSymbFlag fl =
			OutputSymbol::ParseOutputOption(output_format);

		if (fl == osf_none) {
			cerr << "oprofpp: invalid --output-format flags.\n";
			OutputSymbol::ShowHelp();
			exit(EXIT_FAILURE);
		}

		output_format_flags = static_cast<OutSymbFlag>(output_format_flags | fl);
	}

	counter = counter_mask(ctr_str);

	opp_treat_options(file, image_file, sample_file,
			  counter, sort_by_counter);
}

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
	vector<const symbol_entry *> symbols =
		samples.select_symbols(sort_by_ctr, 0.0, false);

	out.Output(cout, symbols, reverse_sort == 0);
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
	vector<const symbol_entry *> symbols =
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
	const symbol_entry * symb = samples.find_symbol(symbol);
	if (symb == 0) {
		cerr << "oprofpp: symbol \"" << symbol
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
			  const opp_samples_files & samples_files,
			  int sort_by_ctr)
{
	static gmon_hdr hdr = { { 'g', 'm', 'o', 'n' }, GMON_VERSION, {0,0,0,},}; 
	FILE *fp; 
	u32 start, end;
	uint j;
	u32 low_pc;
	u32 high_pc;
	u16 * hist;
	u32 histsize;

	fp=op_open_file(gproffile.c_str(), "w");

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

/**
 * samples_file_exist - test for a samples file existence
 * @param filename the base samples filename
 *
 * return true if filename exist
 */
static bool file_exist(const std::string & filename)
{
	ifstream in(filename.c_str());
	return in;
}

/**
 * main
 */
int main(int argc, char const *argv[])
{
	string image_file;
	string sample_file;
	int counter = 0;

	opp_get_options(argc, argv, image_file, sample_file, counter);

	if (!gproffile.empty()) {
		opp_samples_files samples_files(sample_file, counter);
		check_mtime(samples_files, image_file);

		op_bfd abfd(samples_files.is_kernel(), image_file);
		samples_files.set_start_offset(abfd.get_start_offset());
		do_dump_gprof(abfd, samples_files, sort_by_counter);
		return 0;
	}

	bool add_zero_sample_symbols = list_all_symbols_details == 0;
	output_format_flags = 
		static_cast<OutSymbFlag>(output_format_flags | osf_show_all_counters);
	if (!symbol.empty() || list_all_symbols_details)
		output_format_flags = 
			static_cast<OutSymbFlag>(output_format_flags | osf_details);

	/* create the file list of samples files we will use (w/o the
	 * #counter_nr suffix) */
	list<string> filelist;
	if (show_shared_libs) {
		string const dir = dirname(sample_file);
		string const name = basename(sample_file);
 
		get_sample_file_list(filelist, dir, name + "}}}*");
	}

	samples_container_t samples(add_zero_sample_symbols,
				    output_format_flags, counter);

	filelist.push_front(sample_file);

	bool first_file = true;

	list<string>::const_iterator it;
	for (it = filelist.begin() ; it != filelist.end() ; ++it) {
		string const dir = dirname(sample_file);
		// the file list is created with /base_dir/* pattern but with
		// the lazilly samples creation it is perfectly correct for one
		// samples file belonging to counter 0 exist and not exist for
		// counter 1, so we must filter them. It is also valid when
		// profiling with --separate-samples to get samples from
		// shared libs but to do not get samples for the app itself
		int i;
		for (i = 0 ; i < OP_MAX_COUNTERS ; ++i) {
			if ((counter & (1 << i)) != 0) {
				std::ostringstream s;
				s << *it << '#' << i;
				if (file_exist(s.str()) == true) {
					break;
				}
			}
		}

		if (i < OP_MAX_COUNTERS) {
			opp_samples_files samples_files(*it, counter);


			// the first opened file is treated specially because
			// user can specify the image name for this sample
			// file on command line else must deduce the image
			// name from the samples file name
			if (it == filelist.begin()) {
				check_mtime(samples_files, image_file);

				op_bfd abfd(samples_files.is_kernel(), image_file);

				samples_files.set_start_offset(abfd.get_start_offset());

				samples.add(samples_files, abfd);
			} else {
				string app_name;
				string lib_name;
				app_name = extract_app_name(*it, lib_name);

				check_mtime(samples_files, image_file);

				op_bfd abfd(samples_files.is_kernel(), demangle_filename(lib_name));
				samples_files.set_start_offset(abfd.get_start_offset());

				samples.add(samples_files, abfd);
			}

			if (first_file == true) {
				samples_files.output_header();
				first_file = false;
			}
		}
	}

	if (first_file == true) {
		quit_error("oprofpp: Cannot locate any samples file.\n");
	}

	OutputSymbol out(samples, counter);
	out.SetFlag(output_format_flags);

	if (list_symbols)
		do_list_symbols(samples, out, sort_by_counter);
	else if (!symbol.empty())
		do_list_symbol(samples, out);
	else if (list_all_symbols_details)
		do_list_symbols_details(samples, out, sort_by_counter);

	return 0;
}

