/* $Id: oprofpp.h,v 1.32 2001/12/27 21:16:09 phil_e Exp $ */
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

#ifndef OPROFPP_H
#define OPROFPP_H

#include <bfd.h>
#include <popt.h>
 
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>  
#include <fcntl.h> 
#include <errno.h> 

#include <sys/types.h> 
#include <sys/stat.h> 
#include <sys/mman.h>

#include <vector>
#include <string>

#include "../dae/opd_util.h"
#include "../op_user.h"
#include "../version.h"

/* missing from libiberty.h */
#ifndef DMGL_PARAMS
# define DMGL_PARAMS     (1 << 0)        /* Include function args */
#endif 
#ifndef DMGL_ANSI 
# define DMGL_ANSI       (1 << 1)        /* Include const, volatile, etc */
#endif

#ifdef __cplusplus
extern "C" {
#endif

char *cplus_demangle (const char *mangled, int options);

#ifdef __cplusplus
}
#endif

#define verbprintf(args...) \
	do { \
		if (verbose) \
			printf(args); \
	} while (0)

void opp_treat_options(const char * filename, poptContext optcon,
		       string & image_file, string & sample_file);
std::string demangle_symbol(const char* symbol);
void quit_error(poptContext optcon, char const *err);
std::string demangle_filename(const std::string & samples_filename);
bool is_excluded_symbol(const std::string & symbol);

// defined in oprofpp_util.cpp
extern int verbose;
extern int demangle;
extern char const *samplefile;
extern char *basedir;
extern const char *imagefile;
extern const char * exclude_symbols_str;
extern int list_all_symbols_details;
extern int ctr;

//---------------------------------------------------------------------------
// A simple container of counter.
class counter_array_t {
 public:
	counter_array_t();

	u32 operator[](size_t index) const {
		return value[index];
	}

	u32 & operator[](size_t index) {
		return value[index];
	}

	counter_array_t & operator+=(const counter_array_t &);

 private:
	u32 value[OP_MAX_COUNTERS];
};

struct opp_bfd {
	opp_bfd(const opd_header * header, uint nr_samples, const string & filename);
	~opp_bfd();

	bool get_linenr(uint sym_idx, uint offset, 
			const char*& filename, unsigned int& linenr) const;
	void output_linenr(uint sym_idx, uint offset) const;
	void get_symbol_range(uint sym_idx, u32 & start, u32 & end) const;
	int symbol_index(const char* symbol) const;

	u32 sym_offset(uint num_symbols, u32 num) const;

	bool have_debug_info() const;

	bfd *ibfd;
	// sorted vector of interesting symbol.
	std::vector<asymbol*> syms;
	// vector of symbol filled by the bfd lib. Call to bfd lib must this
	// instead of the syms vector.
	asymbol **bfd_syms;
	// image file such the linux kernel need than all vma are offset
	// by this value.
	u32 sect_offset;

private:
	uint nr_samples;
	// ctor helper
	void open_bfd_image(const string & file_name, bool is_kernel);
	bool get_symbols();
};

/* if entry i is invalid all members are set to zero except fd[i] set to -1 */
struct opp_samples_files {
	opp_samples_files(const std::string & sample_file);
	~opp_samples_files();

	void do_list_all_symbols_details(opp_bfd & abfd) const;
	void do_list_symbol_details(opp_bfd & abfd, uint sym_idx) const;
	void do_dump_gprof(opp_bfd & abfd) const;
	void do_list_symbols(opp_bfd & abfd) const;
	void do_list_symbol(opp_bfd & abfd) const;

	/**
	 * is_open - test if a samples file is open
	 * @index: index of the samples file to check.
	 *
	 * return true if the samples file @index is open
	 */ 
	bool is_open(int index) const {
		return samples[index] != 0;
	}
 
	/**
	 * samples_count - check if samples are available
	 * @index: index of the samples files
	 * @samples_nr: number of the samples to test.
	 *
	 * return the number of samples for samples file @index
	 * at position @sample_nr. return 0 if the samples file
	 * is close
	 */
	uint samples_count(int index, int sample_nr) const {
		return is_open(index) ? samples[index][sample_nr].count : 0;
	}
 
	bool accumulate_samples(counter_array_t& counter, uint vma) const;

	void output_header() const;

	opd_fentry *samples[OP_MAX_COUNTERS];	// header + sizeof(header)
	opd_header *header[OP_MAX_COUNTERS];	// mapping begin here
	char *ctr_name[OP_MAX_COUNTERS];
	char *ctr_desc[OP_MAX_COUNTERS];
	char *ctr_um_desc[OP_MAX_COUNTERS];
	fd_t fd[OP_MAX_COUNTERS];
	// This do not include the header size
	size_t size[OP_MAX_COUNTERS];
	uint nr_counters;
	// cached value: index to the first opened file, setup as nearly as we
	// can in ctor.
	int first_file;
	uint nr_samples;

private:
	void output_event(int i) const;

	// ctor helper
	void open_samples_file(const string & sample_file, u32 counter,
			       bool can_fail);
	void check_event(int i);
};

#endif /* OPROFPP_H */
