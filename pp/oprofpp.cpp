/* $Id: oprofpp.cpp,v 1.28 2002/03/03 01:18:19 phil_e Exp $ */
/* COPYRIGHT (C) 2000 THE VICTORIA UNIVERSITY OF MANCHESTER and John Levon
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
 */

// FIXME: printf -> ostream (and elsewhere) 
#include <algorithm>
#include <sstream>
#include <list>

#include "oprofpp.h"
#include "../util/op_popt.h"
#include "../util/file_manip.h"
#include "opf_filter.h"

using std::string;
using std::vector;
using std::cout;
using std::cerr;
 
static int ctr = -1;
static int showvers;
static char *gproffile;
static char *symbol;
static int list_symbols;
static int output_linenr_info;
static int show_shared_libs;
static char * output_format;

static OutSymbFlag output_format_flags;

static poptOption options[] = {
	{ "samples-file", 'f', POPT_ARG_STRING, &samplefile, 0, "image sample file", "file", },
	{ "image-file", 'i', POPT_ARG_STRING, &imagefile, 0, "image file", "file", },
	{ "list-symbols", 'l', POPT_ARG_NONE, &list_symbols, 0, "list samples by symbol", NULL, },
	{ "dump-gprof-file", 'g', POPT_ARG_STRING, &gproffile, 0, "dump gprof format file", "file", },
	{ "list-symbol", 's', POPT_ARG_STRING, &symbol, 0, "give detailed samples for a symbol", "symbol", },
	{ "demangle", 'd', POPT_ARG_NONE, &demangle, 0, "demangle GNU C++ symbol names", NULL, },
	{ "counter", 'c', POPT_ARG_INT, &ctr, 0, "which counter to use", "counter number", }, 
	{ "version", 'v', POPT_ARG_NONE, &showvers, 0, "show version", NULL, },
	{ "verbose", 'V', POPT_ARG_NONE, &verbose, 0, "verbose output", NULL, },
	{ "base-dir", 'b', POPT_ARG_STRING, &basedir, 0, "base directory of profile daemon", NULL, }, 
	{ "list-all-symbols-details", 'L', POPT_ARG_NONE, &list_all_symbols_details, 0, "list samples for all symbols", NULL, },
	{ "output-linenr-info", 'o', POPT_ARG_NONE, &output_linenr_info, 0, "output filename:linenr info", NULL },
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
 * @argc: program arg count
 * @argv: program arg array
 * @image_file: where to store the image filename
 * @sample_file: ditto for sample filename
 * @counter: where to put the counter command line argument
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
	const char *file;
	
	optcon = opd_poptGetContext(NULL, argc, argv, options, 0);

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
 
	/* non-option file, either a sample or binary image file */
	file = poptGetArg(optcon);

	if (show_shared_libs)
		output_format_flags = static_cast<OutSymbFlag>(output_format_flags | osf_image_name);
	if (output_linenr_info)
		output_format_flags = static_cast<OutSymbFlag>(output_format_flags | osf_linenr_info);

	if (output_format == 0) {
		output_format = "vspn";
	} else {
		if (!list_symbols && !list_all_symbols_details && !symbol) {
			quit_error(optcon, "oprofpp: --output-format can be used only without --list-symbols or --list-all-symbols-details.\n");
		}
	}

	if (list_symbols || list_all_symbols_details || symbol) {
		OutSymbFlag fl = ParseOutputOption(output_format);
		if (fl == osf_none) {
			cerr << "oprofpp: illegal --output-format flags.\n";
			OutputSymbol::ShowHelp();
			exit(EXIT_FAILURE);
		}

		output_format_flags = static_cast<OutSymbFlag>(output_format_flags | fl);
	}

	counter = ctr;
	opp_treat_options(file, optcon, image_file, sample_file, counter);

	poptFreeContext(optcon);
}

/**
 * printf_symbol - output a symbol name
 * @name: verbatim symbol name
 *
 * Print the symbol name to stdout, demangling
 * if if necessary.
 */
void printf_symbol(const char *name)
{
	std::string unmangled = demangle_symbol(name);

	printf("%s", unmangled.c_str());
}

/**
 * ouput_linenr - lookup and output linenr info from a vma address
 * in a given section to standard output.
 * @sym_idx: the symbol number
 * @offset: offset
 *
 */
void opp_bfd::output_linenr(uint sym_idx, uint offset) const
{
	const char *filename;
	unsigned int line;

	if (!output_linenr_info)
		return;

	if (get_linenr(sym_idx, offset, filename, line))
		printf ("%s:%u ", filename, line);
	else
		printf ("??:0 ");
}

/**
 * do_list_symbols - list symbol samples for an image
 * @abfd: the bfd object from where come the samples
 *
 * Lists all the symbols in decreasing sample count
 * order, to standard out.
 */
void opp_samples_files::do_list_symbols(opp_bfd & abfd) const
{
	samples_files_t samples;

	samples.add(*this, abfd, true, false, show_shared_libs, counter);

	vector<const symbol_entry *> symbols;

	samples.select_symbols(symbols, counter, 0.0, false);

	OutputSymbol out(samples, counter);

	out.SetFlag(output_format_flags);

	out.Output(cout, symbols, true);
}
 
/**
 * do_list_symbol - list detailed samples for a symbol
 * @abfd: the bfd object from where come the samples
 *
 * the global variable @symbol is used to list all
 * the samples for this symbol from the image 
 * specified by @abfd.
 */
void opp_samples_files::do_list_symbol(opp_bfd & abfd) const
{
	int i = abfd.symbol_index(symbol);
	if (i < 0) {
		cerr << "oprofpp: symbol \"" << symbol
		     << "\" not found in image file.\n";
		return;
	}

	samples_files_t samples;

	samples.add(*this, abfd, true, true, false, counter);

	const symbol_entry * symb = samples.find_symbol(abfd.syms[i]->value +
						abfd.syms[i]->section->vma);
	if (symb == 0) {
		cerr << "oprofpp: symbol \"" << symbol
		     << "\" not found in samples container file.\n";
		return;
	}

	OutputSymbol out(samples, counter);

	out.SetFlag(output_format_flags);
	out.SetFlag(osf_details);

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
 * @abfd: the bfd object from where come the samples
 *
 * Dump gprof-format samples for this sample file and
 * counter specified ctr to the file specified by gproffile.
 *
 * this use the grpof format <= gcc 3.0
 */
void opp_samples_files::do_dump_gprof(opp_bfd & abfd) const
{
	static gmon_hdr hdr = { { 'g', 'm', 'o', 'n' }, GMON_VERSION, {0,0,0,},}; 
	FILE *fp; 
	u32 start, end;
	uint i, j;
	u32 low_pc = (u32)-1;
	u32 high_pc = 0;
	u16 * hist;
	u32 histsize;

	fp=opd_open_file(gproffile, "w");

	opd_write_file(fp,&hdr, sizeof(gmon_hdr));

	opd_write_u8(fp, GMON_TAG_TIME_HIST);

	for (i = 0; i < abfd.syms.size(); i++) {
		if (is_excluded_symbol(abfd.syms[i]->name))
			continue;

		start = abfd.syms[i]->value + abfd.syms[i]->section->vma;
		if (i == abfd.syms.size() - 1) {
			abfd.get_symbol_range(i, start, end);
			end -= start;
			start = abfd.syms[i]->value + abfd.syms[i]->section->vma;
			end += start;
		} else
			end = abfd.syms[i+1]->value + abfd.syms[i+1]->section->vma;
 
		if (start < low_pc)
			low_pc = start;
		if (end > high_pc)
			high_pc = end;
	}

	histsize = ((high_pc - low_pc) / MULTIPLIER) + 1; 
 
	opd_write_u32_he(fp, low_pc);
	opd_write_u32_he(fp, high_pc);
	/* size of histogram */
	opd_write_u32_he(fp, histsize);
	/* profiling rate */
	opd_write_u32_he(fp, 1);
	opd_write_file(fp, "samples\0\0\0\0\0\0\0\0", 15); 
	/* abbreviation */
	opd_write_u8(fp, '1');

	hist = (u16*)xcalloc(histsize, sizeof(u16)); 
 
	for (i = 0; i < abfd.syms.size(); i++) {
		abfd.get_symbol_range(i, start, end); 
		for (j = start; j < end; j++) {
			u32 count;
			u32 pos;
			pos = (abfd.sym_offset(i, j) + abfd.syms[i]->value + abfd.syms[i]->section->vma - low_pc) / MULTIPLIER; 

			/* opp_get_options have set ctr to one value != -1 */
			count = samples_count(counter, j);

			if (pos >= histsize) {
				fprintf(stderr, "Bogus histogram bin %u, larger than %u !", pos, histsize);
				continue;
			}

			if (hist[pos] + count > (u16)-1) {
				printf("Warning: capping sample count by %u samples for "
					"symbol \"%s\"\n", hist[pos] + count - ((u16)-1),
					abfd.syms[i]->name);
				hist[pos] = (u16)-1;
			} else {
				hist[pos] += (u16)count;
			}
		}
	}

	opd_write_file(fp, hist, histsize * sizeof(u16));
	opd_close_file(fp);

	free(hist);
}

/**
 * do_list_all_symbols_details - list all samples for all symbols.
 * @abfd: the bfd object from where come the samples
 *
 * Lists all the samples for all the symbols, from the image specified by
 * @abfd, in increasing order of vma, to standard out.
 */
void opp_samples_files::do_list_all_symbols_details(opp_bfd & abfd) const
{
	samples_files_t samples;

	samples.add(*this, abfd, false, true, show_shared_libs, counter);

	vector<const symbol_entry *> symbols;

	samples.select_symbols(symbols, first_file, 0.0, false, true);

	OutputSymbol out(samples, -1);

	out.SetFlag(output_format_flags);

	// this flag are implicit for oprofpp -L
	out.SetFlag(osf_show_all_counters);
	out.SetFlag(osf_details);

	out.Output(cout, symbols, false);
}

/**
 * output_event - output a counter setup
 * @i: counter number
 *
 * output to stdout a description of an event.
 */
void opp_samples_files::output_event(int i) const
{
	printf("Counter %d counted %s events (%s) with a unit mask of "
	       "0x%.2x (%s) count %u\n", 
	       i, ctr_name[i], ctr_desc[i], header[i]->ctr_um,
	       ctr_um_desc[i] ? ctr_um_desc[i] : "Not set", 
	       header[i]->ctr_count);
}

/**
 * output_header() - output counter setup
 *
 * output to stdout the cpu type, cpu speed
 * and all counter description available
 */
void opp_samples_files::output_header() const
{
	op_cpu cpu = static_cast<op_cpu>(header[first_file]->cpu_type);
 
	printf("Cpu type: %s\n", op_get_cpu_type_str(cpu));

	printf("Cpu speed was (MHz estimation) : %f\n", header[first_file]->cpu_speed);

	for (uint i = 0 ; i < OP_MAX_COUNTERS; ++i) {
		if (fd[i] != -1)
			output_event(i);
	}
}

/**
 * main
 */
int main(int argc, char const *argv[])
{
	string image_file;
	string sample_file;
	int counter = -1;
	opp_get_options(argc, argv, image_file, sample_file, counter);

	opp_samples_files samples_files(sample_file, counter);
	opp_bfd abfd(samples_files.header[samples_files.first_file],
		     samples_files.nr_samples, image_file);

	samples_files.output_header();

	if (list_symbols)
		samples_files.do_list_symbols(abfd);
	else if (symbol)
		samples_files.do_list_symbol(abfd);
	else if (gproffile)
		samples_files.do_dump_gprof(abfd);
	else if (list_all_symbols_details)
		samples_files.do_list_all_symbols_details(abfd);

	return 0;
}

