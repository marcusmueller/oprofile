/* $Id: oprofpp.cpp,v 1.24 2002/01/26 03:30:15 phil_e Exp $ */
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
 
// ugly global var for opf_container.cpp
uint op_nr_counters;
 
static int showvers;
static char *gproffile;
static char *symbol;
static int list_symbols;
static int output_linenr_info;
static int show_shared_libs;

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
	POPT_AUTOHELP
	{ NULL, 0, 0, NULL, 0, NULL, NULL, },
};

/**
 * get_options - process command line
 * @argc: program arg count
 * @argv: program arg array
 * @image_file: where to store the image filename
 * @sample_file: ditto for sample filename
 *
 * Process the arguments, fatally complaining on
 * error. Only a part of arguments analysing is
 * made here. The filename checking is deferred to
 * opp_treat_options().
 *
 */
static void opp_get_options(int argc, const char **argv, string & image_file,
			    string & sample_file)
{
	poptContext optcon;
	const char *file;
	
	optcon = opd_poptGetContext(NULL, argc, argv, options, 0);

	if (showvers) {
		printf(VERSION_STRING " compiled on " __DATE__ " " __TIME__ "\n");
		exit(EXIT_SUCCESS);
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

	opp_treat_options(file, optcon, image_file, sample_file);

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
	// TODO avoid that: set the ugly global var for opf_container.cpp
	op_nr_counters = nr_counters;
	samples_files_t samples;

	samples.add(*this, abfd, true, false, show_shared_libs);

	vector<const symbol_entry *> symbols;

	samples.select_symbols(symbols, ctr, 0.0, false);

	u32 total_count = samples.samples_count(ctr);

	// const_reverse_iterator can't work :/
	vector<const symbol_entry*>::reverse_iterator it;
	for (it = symbols.rbegin(); it != symbols.rend(); ++it) {
		if (show_shared_libs)
			printf("%s ", (*it)->sample.file_loc.image_name.c_str());
		if (output_linenr_info)
			printf("%s:%u ",
			       (*it)->sample.file_loc.filename.c_str(),
			       (*it)->sample.file_loc.linenr);

		printf("%s", (*it)->name.c_str());

		u32 count = (*it)->sample.counter[ctr];

		if (count) {
			printf("[0x%.8lx]: %2.4f%% (%u samples)\n", 
			       (*it)->sample.vma,
			       (((double)count) / total_count)*100.0, 
			       count);
		} else {
			printf(" (0 samples)\n");
		}
	}
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
	u32 start, end;
	u32 j;

	int i = abfd.symbol_index(symbol);
	if (i < 0) {
		fprintf(stderr, "oprofpp: symbol \"%s\" not found in image file.\n", symbol);
		return;
	}

	printf("Samples for symbol \"%s\" in image %s\n", symbol, abfd.ibfd->filename);

	abfd.get_symbol_range(i, start, end);
	for (j = start; j < end; j++) {
		uint sample_count = samples_count(ctr, j);
		if (!sample_count)
			continue;

		abfd.output_linenr(i, j);
		printf("%s+%x/%x:\t%u\n", symbol, abfd.sym_offset(i, j), 
		       end - start, sample_count);
	}
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
			count = samples_count(ctr, j);

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
	// TODO avoid that: set the ugly global var for opf_container.cpp
	op_nr_counters = nr_counters;

	samples_files_t samples;

	samples.add(*this, abfd, false, true, show_shared_libs);

	vector<const symbol_entry *> symbols;

	samples.select_symbols(symbols, first_file, 0.0, false, true);

	vector<const symbol_entry*>::const_iterator it;
	for (it = symbols.begin(); it != symbols.end(); ++it) {

		if (show_shared_libs)
			printf("%s ", (*it)->sample.file_loc.image_name.c_str());

		if (output_linenr_info)
			printf("%s:%u ",
			       (*it)->sample.file_loc.filename.c_str(),
			       (*it)->sample.file_loc.linenr);

		printf("%.8lx ", (*it)->sample.vma);
		for (uint k = 0 ; k < nr_counters ; ++k)
			printf("%u ", (*it)->sample.counter[k]);

		printf("%s\n", (*it)->name.c_str());

		for (size_t cur = (*it)->first ; cur != (*it)->last ; ++cur) {
			const sample_entry & sample = samples.get_samples(cur);

			printf(" ");

			if (output_linenr_info)
				printf("%s:%u ",
				       sample.file_loc.filename.c_str(),
				       sample.file_loc.linenr);
			printf("%.8lx", sample.vma);

			for (uint k = 0; k < nr_counters; ++k)
				printf(" %u", sample.counter[k]);
			printf("\n");
		}
	}
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
	opp_get_options(argc, argv, image_file, sample_file);

	opp_samples_files samples_files(sample_file);
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

