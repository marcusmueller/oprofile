/* $Id: oprofpp.h,v 1.48 2002/03/22 21:18:43 phil_e Exp $ */
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

#include <iostream>
#include <vector>
#include <string>

#include "../dae/opd_util.h"
#include "../op_user.h"
#include "../version.h"

class symbol_entry;

/* missing from libiberty.h */
/*@{\name demangle option parameter */
#ifndef DMGL_PARAMS
# define DMGL_PARAMS     (1 << 0)        /**< Include function args */
#endif 
#ifndef DMGL_ANSI 
# define DMGL_ANSI       (1 << 1)        /**< Include const, volatile, etc */
#endif
/*@}*/

#ifdef __cplusplus
extern "C" {
#endif

/** return a dynamically allocated string containing the demangled name */
char *cplus_demangle (const char *mangled, int options);

#ifdef __cplusplus
}
#endif

// To avoid doxygen warning
#define OP_VERBPRINTF_FORMAT __attribute__((format (printf, 1, 2)))

/* oprofpp_util.cpp */

/** like printf but only output the message if the global variable vebose
 * is non-zero */
void verbprintf(const char* args, ...) OP_VERBPRINTF_FORMAT;

/**
 * \param out output to this ostream
 * \param cpu_type the cpu_type
 * \param type event type
 * \param um the unit mask
 * \param count events count before overflow
 *
 * output a human readable form of an event setting
 */
void op_print_event(ostream & out, int i, op_cpu cpu_type,
		    u8 type, u8 um, u32 count);

/**
 * process command line options
 * \param file a filename passed on the command line, can be NULL
 * \param i counter number
 * \param optcon poptContext to allow better message handling
 * \param image_file where to store the image file name
 * \param sample_file ditto for sample filename
 * \param counter where to put the counter command line argument
 *
 * Process the arguments, fatally complaining on error. 
 *
 * file is considered as a sample file if it contains at least one
 * OPD_MANGLE_CHAR else it is an image file. If no image file is given
 * on command line the sample file name is un-mangled -after- stripping
 * the optionnal "\#nr" suffixe. This give some limitations on the image
 * filename.
 *
 * all filename checking is made here only with a syntactical approch. (ie
 * existence of filename is not tested)
 *
 * post-condition: sample_file and image_file are setup
 */
void opp_treat_options(const char * filename, poptContext optcon,
		       std::string & image_file, std::string & sample_file,
		       int & counter, int & sort_by_counter);

/**
 * \param symbol: the symbol name to demangle
 *
 * demangle the symbol name if the global global variable demangle is true.
 * else return the name w/o demangling. The demangled name lists the parameters
 * and type qualifiers such as "const". Return the un-mangled name
 */
std::string demangle_symbol(const char* symbol);

/**
 * quit with error
 * \param err error to show
 *
 * err may be NULL
 */
void quit_error(poptContext optcon, char const *err);

/**
 * convert a sample filenames into the related image file name
 * \param sample_filename the samples image filename
 *
 * if samples_filename does not contain any %OPD_MANGLE_CHAR
 * the string samples_filename itself is returned.
 */
std::string demangle_filename(const std::string & samples_filename);

/**
 * check if the symbol is in the exclude list
 * \param symbol symbol name to check
 *
 * return true if symbol is in the list of excluded symbol
 */
bool is_excluded_symbol(const std::string & symbol);

/**
 * check coherence between two headers.
 * \param f1 first header
 * \param f2 second header
 *
 * verify that header f1 and f2 are coherent.
 * all error are fatal
 */
void check_headers(const opd_header * f1, const opd_header * f2);

/**
 * validate the counter number
 * \param counter_mask bit mask specifying the counter nr to use
 * \param sort_by the counter nr from which we sort
 *
 * all error are fatal
 */
void validate_counter(int counter_mask, int & sort_by);

/**
 * given a --counter=0,1,..., option parameter return a mask
 * representing each counter. Bit i is on if counter i was specified.
 * So we allow up to sizeof(uint) * CHAR_BIT different counter
 */
uint counter_mask(const std::string &);

/** control the behavior of verbprintf() */
extern int verbose;
/** control the behavior of demangle_symbol() */
extern int demangle;

/** command line option specifying a sample filename */
extern char const *samplefile;
/** command line option specifying an image filename */
extern const char *imagefile;
/** command line option which specify the base directory of samples files */
extern char *basedir;
/** command line option specifying the set of symbols to ignore */
extern const char * exclude_symbols_str;

//---------------------------------------------------------------------------
/** A simple container of counter. Can hold OP_MAX_COUNTERS counters */
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

/** Encapsulation of a bfd object. Simplify open/close of bfd, enumerating
 * symbols and retrieving informations for symbols or vma. */
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

	/**
	 * symbol_size - return the size of a symbol
	 * \param index symbol index
	 */
	size_t symbol_size(uint sym_idx) const;

	/** Returns true if the underlined bfd object contains debug info */
	bool have_debug_info() const;

	// TODO: avoid this two public data members
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

/** A class to store one samples file */
/* TODO: misnamed, it exist a samples_files_t class which is very confusing */
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

/** Store multiple samples files belonging to the same image and the same
 * session */
/* I think this would be rewritten to use an array of samples_file_t */
struct opp_samples_files {
	/**
	 * \param sample_file name of sample file to open w/o the #nr suffix
	 * \param counter a bit mask specifying which sample file to open
	 *
	 * Open all samples files specified through sample_file and counter.
	 * Currently all error are fatal
	 */
	opp_samples_files(const std::string & sample_file, int counter);

	/** Close all opened samples files and free all related resource. */
	~opp_samples_files();

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
	 * \param index index of the samples files
	 * \param sample_nr number of the samples to test.
	 *
	 * return the number of samples for samples file index at position
	 * sample_nr. return 0 if the samples file is close or there is no
	 * samples at position sample_nr
	 */
	uint samples_count(int index, int sample_nr) const {
		return is_open(index) ? samples[index][sample_nr].count : 0;
	}

	/**
	 * \param counter where to accumulate the samples
	 * \param index index of the samples.
	 *
	 * return false if no samples has been found
	 */

	bool accumulate_samples(counter_array_t& counter, uint vma) const;
	/**
	 * \param counter where to accumulate the samples
	 * \param start start index of the samples.
	 * \param end end index of the samples.
	 *
	 * return false if no samples has been found
	 */
	bool accumulate_samples(counter_array_t& counter,
				uint start, uint end) const;

	// this look like a free fun
	void output_header() const;

	// TODO privatisze as we can.
	/* if entry i is invalid all members are set to zero except fd[i]
	 * set to -1 */
	opd_fentry *samples[OP_MAX_COUNTERS];	// header + sizeof(header)
	opd_header *header[OP_MAX_COUNTERS];	// mapping begin here
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
	size_t counter_mask;

private:
	void output_event(int i) const;

	// ctor helper
	void open_samples_file(u32 counter, bool can_fail);
	void check_event(int i);
};

#endif /* OPROFPP_H */
