/* $Id: oprofpp.h,v 1.44 2002/03/19 05:41:25 phil_e Exp $ */
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

class symbol_entry;

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

void verbprintf(const char* args, ...) __attribute__((format (printf, 1, 2)));
void opp_treat_options(const char * filename, poptContext optcon,
		       std::string & image_file, std::string & sample_file,
		       int & counter, int & sort_by_counter);
std::string demangle_symbol(const char* symbol);
void quit_error(poptContext optcon, char const *err);
std::string demangle_filename(const std::string & samples_filename);
bool is_excluded_symbol(const std::string & symbol);
void check_headers(const opd_header * f1, const opd_header * f2);
void validate_counter(int counter_mask, int & sort_by);

/// given a --counter=0,1,..., option parameter return a mask representing
/// each counter. Bit i is on if counter i was specified in the option string
uint counter_mask(const std::string &);

// defined in oprofpp_util.cpp
extern int verbose;
extern int demangle;
extern char const *samplefile;
extern char *basedir;
extern const char *imagefile;
extern const char * exclude_symbols_str;
extern int list_all_symbols_details;

//---------------------------------------------------------------------------
/** A simple container of counter, holding OP_MAX_COUNTERS counter */
class counter_array_t {
public:
	/** counter_array_t ctor, all counter are initialized to zero */
	counter_array_t();

	/** subscript operator indexed by a counter_nr, no bound check
	 * is performed. */
	u32 operator[](size_t counter_nr) const {
		return value[counter_nr];
	}

	/** subscript operator indexed by a counter_nr, no bound check
	 * is performed. */
	u32 & operator[](size_t counter_nr) {
		return value[counter_nr];
	}

	/** vectorised += operator */
	counter_array_t & operator+=(const counter_array_t & rhs);

private:
	u32 value[OP_MAX_COUNTERS];
};

/** encapsulation of a bfd object, simplifying open/close of bfd, enumerating
 * symbols and retrieving informations on symbols or vma. */
class opp_bfd {
public:
	/**
	 * \param header a valid samples file opd_header
	 * \param nr_samples the number of samples location for this image ie
	 * the number of bytes memory mapped for this image with EXEC right
	 * \param image_file the name of the image file
	 *
	 * All error are fatal.
	 *
	 */
	opp_bfd(const opd_header * header, uint nr_samples,
		const std::string & filename);

	/** close an opended bfd image and free all related resource. */
	~opp_bfd();

	/**
	 * \param sym_idx index of the symbol
	 * \param offset fentry number
	 * \param filename output parameter to store filename
	 * \param linenr output parameter to store linenr.
	 *
	 * retrieve the relevant finename:linenr information for the sym_idx
	 * at offset. If the lookup fail return false. In some case this
	 * function can retrieve the filename and return true but fail to
	 * retrieve the linenr and so can return zero in linenr
	 */
	bool get_linenr(uint sym_idx, uint offset, 
			const char*& filename, unsigned int& linenr) const;

	/**
	 * \param sym_idx symbol index
	 * \param start pointer to start var
	 * \param end pointer to end var
	 *
	 * Calculates the range of sample file entries covered by sym. start
	 * and end will be filled in appropriately. If index is the last entry
	 * in symbol table, all entries up to the end of the sample file will
	 * be used.  After calculating start and end they are sanitized
	 *
	 * All error are fatal.
	 */
	void get_symbol_range(uint sym_idx, u32 & start, u32 & end) const;

	/** \param name the symbol name
	 *
	 * find and return the index of a symbol else return -1
	 */
	int symbol_index(const char* symbol) const;

	/**
	 * sym_offset - return offset from a symbol's start
	 * \param sym_index symbol number
	 * \param num number of fentry
	 *
	 * Returns the offset of a sample at position num
	 * in the samples file from the start of symbol sym_idx.
	 */
	u32 sym_offset(uint num_symbols, u32 num) const;

	/** Returns true if the underlined bfd object contains debug info */
	bool have_debug_info() const;

	// TODO: avoid this two pulbic data members
	bfd *ibfd;
	// sorted vector of interesting symbol.
	std::vector<asymbol*> syms;
private:
	// vector of symbol filled by the bfd lib. Call to bfd lib must use
	// this instead of the syms vector.
	asymbol **bfd_syms;
	// image file such the linux kernel need than all vma are offset
	// by this value.
	u32 sect_offset;
	// nr of samples.
	uint nr_samples;
	// ctor helper
	void open_bfd_image(const std::string & file_name, bool is_kernel);
	bool get_symbols();
};

/**
 * A class to store one samples file
 */
struct samples_file_t
{
	samples_file_t(const std::string & filename);
	~samples_file_t();

	bool check_headers(const samples_file_t & headers) const;

	u32 count(uint start, uint end) const;

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
	size_t nr_samples;

private:
	// neither copy-able or copy constructible
	samples_file_t(const samples_file_t &);
	samples_file_t& operator=(const samples_file_t &);
};

/* if entry i is invalid all members are set to zero except fd[i] set to -1 */
/* It will be nice if someone redesign this to use samples_file_t for the
 * internal implementation of opp_samples_files */
struct opp_samples_files {
	opp_samples_files(const std::string & sample_file, int counter);
	~opp_samples_files();

	void do_list_symbols_details(opp_bfd & abfd, int sort_by_ctr) const;
	void do_dump_gprof(opp_bfd & abfd, int sort_by_ctr) const;
	void do_list_symbols(opp_bfd & abfd, int sort_by_ctr) const;
	void do_list_symbol(opp_bfd & abfd/*, int sort_by_ctr*/) const;

	/**
	 * is_open - test if a samples file is open
	 * \param index index of the samples file to check.
	 *
	 * return true if the samples file index is open
	 */ 
	bool is_open(int index) const {
		return samples[index] != 0;
	}
 
	/**
	 * samples_count - check if samples are available
	 * \param index index of the samples files
	 * \param samples_nr number of the samples to test.
	 *
	 * return the number of samples for samples file index
	 * at position sample_nr. return 0 if the samples file
	 * is close
	 */
	uint samples_count(int index, int sample_nr) const {
		return is_open(index) ? samples[index][sample_nr].count : 0;
	}
 
	bool accumulate_samples(counter_array_t& counter, uint vma) const;
	bool accumulate_samples(counter_array_t& counter,
				uint start, uint end) const;

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
	std::string sample_filename;

	// used in do_list_xxxx/do_dump_gprof.
	int counter;

private:
	void output_event(int i) const;

	// ctor helper
	void open_samples_file(u32 counter, bool can_fail);
	void check_event(int i);
};

#endif /* OPROFPP_H */
