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
#include "op_popt.h"
#include "op_libiberty.h"
#include "op_fileio.h"
#include "file_manip.h"
#include "samples_container.h"

using std::string;
using std::vector;
using std::cout;
using std::cerr;
 
static char * ctr_str;
static int showvers;
static int sort_by_counter = -1;
static char *gproffile;
static char *symbol;
static int list_symbols;
static int output_linenr_info;
static int reverse_sort;
static int show_shared_libs;
static char * output_format;
static int list_all_symbols_details;

static OutSymbFlag output_format_flags;

static poptOption options[] = {
	{ "samples-file", 'f', POPT_ARG_STRING, &samplefile, 0, "image sample file", "file", },
	{ "image-file", 'i', POPT_ARG_STRING, &imagefile, 0, "image file", "file", },
	{ "list-symbols", 'l', POPT_ARG_NONE, &list_symbols, 0, "list samples by symbol", NULL, },
	{ "dump-gprof-file", 'g', POPT_ARG_STRING, &gproffile, 0, "dump gprof format file", "file", },
	{ "list-symbol", 's', POPT_ARG_STRING, &symbol, 0, "give detailed samples for a symbol", "symbol", },
	{ "demangle", 'd', POPT_ARG_NONE, &demangle, 0, "demangle GNU C++ symbol names", NULL, },
	{ "counter", 'c', POPT_ARG_STRING, &ctr_str, 0, "which counter to display", "counter number[,counter nr]", }, 
	{ "sort", 'C', POPT_ARG_INT, &sort_by_counter, 0, "which counter to use for sampels sort", "counter nr", }, 
	{ "version", 'v', POPT_ARG_NONE, &showvers, 0, "show version", NULL, },
	{ "verbose", 'V', POPT_ARG_NONE, &verbose, 0, "verbose output", NULL, },
	{ "list-all-symbols-details", 'L', POPT_ARG_NONE, &list_all_symbols_details, 0, "list samples for all symbols", NULL, },
	{ "output-linenr-info", 'o', POPT_ARG_NONE, &output_linenr_info, 0, "output filename:linenr info", NULL },
	{ "reverse", 'r', POPT_ARG_NONE, &reverse_sort, 0,
	  "reverse sort order", NULL, },
	{ "exclude-symbol", 'e', POPT_ARG_STRING, &exclude_symbols_str, 0, "exclude these comma separated symbols", "symbol_name" },
	{ "show-shared-libs", 'k', POPT_ARG_NONE, &show_shared_libs, 0,
	  "show details for shared libs. Only meaningfull if you have profiled with --separate-samples", NULL, },
	// FIXME: clarify this
	{ "output-format", 't', POPT_ARG_STRING, &output_format, 0,
	  "choose the output format", "output-format strings", },
	POPT_AUTOHELP
	{ NULL, 0, 0, NULL, 0, NULL, NULL, },
};

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
	poptContext optcon;
	char const * file;
	
	optcon = op_poptGetContext(NULL, argc, argv, options, 0);

	if (showvers) {
		show_version(argv[0]);
	}
 
	if (!list_all_symbols_details && !list_symbols && 
	    !gproffile && !symbol)
		quit_error(optcon, "oprofpp: no mode specified. What do you want from me ?\n");

	/* check only one major mode specified */
	if ((list_all_symbols_details + list_symbols + (gproffile != 0) + (symbol != 0)) > 1)
		quit_error(optcon, "oprofpp: must specify only one output type.\n");

	if (output_linenr_info && !list_all_symbols_details && !symbol && !list_symbols)
		quit_error(optcon, "oprofpp: cannot list debug info without -L or -s option.\n");

	if (show_shared_libs && (symbol || gproffile)) {
		quit_error(optcon, "oprofpp: you cannot specify --show-shared-libs with --dump-gprof-file or --list-symbol output type.\n");
	}
 
	if (reverse_sort && !list_symbols)
		quit_error(optcon, "oprofpp: reverse sort can only be used with -l option.\n");
 
	/* non-option file, either a sample or binary image file */
	file = poptGetArg(optcon);

	if (show_shared_libs)
		output_format_flags = static_cast<OutSymbFlag>(output_format_flags | osf_image_name);
	if (output_linenr_info)
		output_format_flags = static_cast<OutSymbFlag>(output_format_flags | osf_linenr_info);

	if (output_format == 0) {
		output_format = "hvspn";
	} else {
		if (!list_symbols && !list_all_symbols_details && !symbol) {
			quit_error(optcon, "oprofpp: --output-format can be used only without --list-symbols or --list-all-symbols-details.\n");
		}
	}

	if (list_symbols || list_all_symbols_details || symbol) {
		OutSymbFlag fl =
			OutputSymbol::ParseOutputOption(output_format);

		if (fl == osf_none) {
			cerr << "oprofpp: invalid --output-format flags.\n";
			OutputSymbol::ShowHelp();
			exit(EXIT_FAILURE);
		}

		output_format_flags = static_cast<OutSymbFlag>(output_format_flags | fl);
	}

	if (!ctr_str)
		ctr_str = "";

	counter = counter_mask(ctr_str);

	opp_treat_options(file, optcon, image_file, sample_file,
			  counter, sort_by_counter);

	poptFreeContext(optcon);
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
	vector<const symbol_entry *> symbols;

	samples.select_symbols(symbols, sort_by_ctr, 0.0, false);

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
	vector<const symbol_entry *> symbols;

	samples.select_symbols(symbols, sort_by_ctr, 0.0, false, true);

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

	fp=op_open_file(gproffile, "w");

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

	if (gproffile) {
		opp_samples_files samples_files(sample_file, counter);
		op_bfd abfd(samples_files, image_file);

		do_dump_gprof(abfd, samples_files, sort_by_counter);
		return 0;
	}

	bool add_zero_sample_symbols = list_all_symbols_details == 0;
	output_format_flags = 
		static_cast<OutSymbFlag>(output_format_flags | osf_show_all_counters);
	if (symbol || list_all_symbols_details)
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
				op_bfd abfd(samples_files, image_file);

				samples.add(samples_files, abfd);
			} else {
				string app_name;
				string lib_name;
				app_name = extract_app_name(*it, lib_name);
				op_bfd abfd(samples_files, demangle_filename(lib_name));

				samples.add(samples_files, abfd);
			}

			if (first_file == true) {
				samples_files.output_header();
				first_file = false;
			}
		}
	}

	if (first_file == true) {
		cerr << "oprofpp: Cannot locate any samples file.\n";
		exit(EXIT_FAILURE);
	}

	OutputSymbol out(samples, counter);
	out.SetFlag(output_format_flags);

	if (list_symbols)
		do_list_symbols(samples, out, sort_by_counter);
	else if (symbol)
		do_list_symbol(samples, out);
	else if (list_all_symbols_details)
		do_list_symbols_details(samples, out, sort_by_counter);

	return 0;
}

