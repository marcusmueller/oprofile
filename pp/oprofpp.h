/* $Id: oprofpp.h,v 1.18 2001/09/21 06:33:17 phil_e Exp $ */
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

void opp_get_options(int argc, char const *argv[]);
char* demangle_symbol(const char* symbol);

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
	opp_bfd(const opd_footer * footer);
	~opp_bfd();

	bool get_linenr(uint sym_idx, uint offset, 
			const char** filename, unsigned int* linenr) ;
	void output_linenr(uint sym_idx, uint offset);
	void get_symbol_range(uint sym_idx, u32 *start, u32 *end) const;
	int symbol_index(const char* symbol) const;

	u32 sym_offset(uint num_symbols, u32 num) const;

	bfd *ibfd;
	std::vector<asymbol*> syms;
	// image file such the linux kernel need than all vma are offset
	// by this value.
	u32 sect_offset;

private:
	// ctor helper
	void open_bfd_image(const char* file, bool is_kernel);
	bool get_symbols();
};

/* if entry i is invalid all members are set to zero except fd[i] set to -1 */
struct opp_samples_files {
	opp_samples_files();
	~opp_samples_files();

	void do_list_all_symbols_details(opp_bfd* abfd) const;
	void do_list_symbol_details(opp_bfd* abfd, uint sym_idx) const;
	void do_dump_gprof(opp_bfd* abfd) const;
	void do_list_symbols(opp_bfd* abfd) const;
	void do_list_symbol(opp_bfd* abfd) const;

	bool is_open(int index) const;
	uint samples_count(int index, int sample_nr) const;
	bool accumulate_samples(counter_array_t& counter, uint vma) const;

	void output_header() const;

	opd_fentry *samples[OP_MAX_COUNTERS];	// footer + sizeof(footer)
	opd_footer *footer[OP_MAX_COUNTERS];	// mapping begin here
	char *ctr_name[OP_MAX_COUNTERS];
	char *ctr_desc[OP_MAX_COUNTERS];
	char *ctr_um_desc[OP_MAX_COUNTERS];
	fd_t fd[OP_MAX_COUNTERS];
	// This do not include the footer size
	size_t size[OP_MAX_COUNTERS];
	uint nr_counters;
	// cached value: index to the first opened file, setup as nearly as we
	// can in ctor.
	int first_file;

private:
	void output_event(int i) const;

	// ctor helper
	void open_samples_file(u32 counter, bool can_fail);
	void check_event(int i);
};

#endif /* OPROFPP_H */
