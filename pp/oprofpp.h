/**
 * \file oprofpp.h
 * Copyright 2002 OProfile authors
 * Read the file COPYING
 *
 * \author John Levon <moz@compsoc.man.ac.uk>
 * \author Philippe Elie <phil_el@wanadoo.fr>
 */

#ifndef OPROFPP_H
#define OPROFPP_H

#include <bfd.h>
 
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

#include "db.h"
#include "op_interface.h"
#include "op_popt.h"
#include "op_sample_file.h"
#include "op_hw_config.h"

class symbol_entry;
class opp_samples_files;

// To avoid doxygen warning
#define OP_VERBPRINTF_FORMAT __attribute__((format (printf, 1, 2)))

/* oprofpp_util.cpp */

/** FIXME: we should be using a proper C++ debug system (and we should
 *  can then be waaay more verbose. Think :

    oper[debug::assembly] << asmtext;

    etc.

 */

/** like printf but only output the message if the global variable vebose
 * is non-zero */
void verbprintf(const char* args, ...) OP_VERBPRINTF_FORMAT;

/**
 * @param out output to this ostream
 * @param cpu_type the cpu_type
 * @param type event type
 * @param um the unit mask
 * @param count events count before overflow
 *
 * output a human readable form of an event setting
 */
void op_print_event(std::ostream & out, int i, op_cpu cpu_type,
		    u8 type, u8 um, u32 count);

/**
 * process command line options
 * @param file a filename passed on the command line, can be NULL
 * @param i counter number
 * @param optcon poptContext to allow better message handling
 * @param image_file where to store the image file name
 * @param sample_file ditto for sample filename
 * @param counter where to put the counter command line argument
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
void opp_treat_options(char const * filename, poptContext optcon,
		       std::string & image_file, std::string & sample_file,
		       int & counter, int & sort_by_counter);

/**
 * quit with error
 * @param err error to show
 *
 * err may be NULL
 */
void quit_error(poptContext optcon, char const *err);

/**
 * convert a sample filenames into the related image file name
 * @param sample_filename the samples image filename
 *
 * if samples_filename does not contain any %OPD_MANGLE_CHAR
 * the string samples_filename itself is returned.
 */
std::string demangle_filename(const std::string & samples_filename);

/**
 * check if the symbol is in the exclude list
 * @param symbol symbol name to check
 *
 * return true if symbol is in the list of excluded symbol
 */
bool is_excluded_symbol(const std::string & symbol);

/**
 * check coherence between two headers.
 * @param f1 first header
 * @param f2 second header
 *
 * verify that header f1 and f2 are coherent.
 * all error are fatal
 */
void check_headers(const opd_header * f1, const opd_header * f2);

/**
 * sanity check of a struct opd_header *
 * @param header a pointer to header to check
 *
 * all error are fatal
 */
void check_event(const opd_header * header);

/**
 * validate the counter number
 * @param counter_mask bit mask specifying the counter nr to use
 * @param sort_by the counter nr from which we sort
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
extern char const *imagefile;
/** command line option specifying the set of symbols to ignore */
extern char const * exclude_symbols_str;

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
	 * @param samples a valid samples file associated with this image
	 * @param image_file the name of the image file
	 *
	 * All error are fatal.
	 *
	 */
	opp_bfd(opp_samples_files& samples, const std::string & filename);

	/** close an opended bfd image and free all related resource. */
	~opp_bfd();

	/**
	 * @param sym_idx index of the symbol
	 * @param offset fentry number
	 * @param filename output parameter to store filename
	 * @param linenr output parameter to store linenr.
	 *
	 * retrieve the relevant finename:linenr information for the sym_idx
	 * at offset. If the lookup fail return false. In some case this
	 * function can retrieve the filename and return true but fail to
	 * retrieve the linenr and so can return zero in linenr
	 */
	bool get_linenr(uint sym_idx, uint offset, 
			const char*& filename, unsigned int& linenr) const;

	/**
	 * @param sym_idx symbol index
	 * @param start pointer to start var
	 * @param end pointer to end var
	 *
	 * Calculates the range of sample file entries covered by sym. start
	 * and end will be filled in appropriately. If index is the last entry
	 * in symbol table, all entries up to the end of the sample file will
	 * be used.  After calculating start and end they are sanitized
	 *
	 * All error are fatal.
	 */
	void get_symbol_range(uint sym_idx, u32 & start, u32 & end) const;

	/** @param name the symbol name
	 *
	 * find and return the index of a symbol else return -1
	 */
	int symbol_index(const char* symbol) const;

	/**
	 * sym_offset - return offset from a symbol's start
	 * @param sym_index symbol number
	 * @param num number of fentry
	 *
	 * Returns the offset of a sample at position num
	 * in the samples file from the start of symbol sym_idx.
	 */
	u32 sym_offset(uint num_symbols, u32 num) const;

	/**
	 * symbol_size - return the size of a symbol
	 * @param index symbol index
	 */
	size_t symbol_size(uint sym_idx) const;

	/** Returns true if the underlined bfd object contains debug info */
	bool have_debug_info() const;

	// TODO: avoid this two public data members
	bfd *ibfd;
	// sorted vector of interesting symbol.
	std::vector<asymbol*> syms;
	// nr of samples.
	uint nr_samples;
private:
	// vector of symbol filled by the bfd lib. Call to bfd lib must use
	// this instead of the syms vector.
	asymbol **bfd_syms;
	// image file such the linux kernel need than all vma are offset
	// by this value.
	u32 sect_offset;
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

	u32 count(uint start) const { 
		return  count(start, start + 1);
	}

	u32 count(uint start, uint end) const;

	const struct opd_header * header() const {
		return (struct opd_header *)db_tree.base_memory;
	}

	// probably needs to be private and create the neccessary member
	// function (not simple getter), make private and compile to see
	// what operation we need later. I've currently not a clear view
	// of what we need
//private:
	db_tree_t db_tree;

	// this offset is zero for user space application and the file pos
	// of text section for kernel and module.
	u32 sect_offset;

private:
	// neither copy-able or copy constructible
	samples_file_t(const samples_file_t &);
	samples_file_t& operator=(const samples_file_t &);
};

/** Store multiple samples files belonging to the same image and the same
 * sessionn can hold OP_MAX_COUNTERS sampels files */
struct opp_samples_files {
	/**
	 * @param sample_file name of sample file to open w/o the #nr suffix
	 * @param counter a bit mask specifying which sample file to open
	 *
	 * Open all samples files specified through sample_file and counter.
	 * Currently all error are fatal
	 */
	opp_samples_files(const std::string & sample_file, int counter);

	/** Close all opened samples files and free all related resource. */
	~opp_samples_files();

	/**
	 * is_open - test if a samples file is open
	 * @param index index of the samples file to check.
	 *
	 * return true if the samples file index is open
	 */ 
	bool is_open(int i) const {
		return samples[i] != 0;
	}
 
	/**
	 * @param index index of the samples files
	 * @param sample_nr number of the samples to test.
	 *
	 * return the number of samples for samples file index at position
	 * sample_nr. return 0 if the samples file is close or there is no
	 * samples at position sample_nr
	 */
	uint samples_count(int i, int sample_nr) const {
		return is_open(i) ? samples[i]->count(sample_nr) : 0;
	}

	/**
	 * @param counter where to accumulate the samples
	 * @param index index of the samples.
	 *
	 * return false if no samples has been found
	 */

	bool accumulate_samples(counter_array_t& counter, uint vma) const;
	/**
	 * @param counter where to accumulate the samples
	 * @param start start index of the samples.
	 * @param end end index of the samples.
	 *
	 * return false if no samples has been found
	 */
	bool accumulate_samples(counter_array_t& counter,
				uint start, uint end) const;

	// this look like a free fun
	void output_header() const;

	/// return a struct opd_header * of the first openened samples file
	const struct opd_header * first_header() const {
		return samples[first_file]->header();
	}

	void set_sect_offset(u32 sect_offset);

	// TODO privatisze as we can.
	samples_file_t * samples[OP_MAX_COUNTERS];
	uint nr_counters;
	std::string sample_filename;

	// used in do_list_xxxx/do_dump_gprof.
	size_t counter_mask;

private:
	// cached value: index to the first opened file, setup as nearly as we
	// can in ctor.
	int first_file;

	// ctor helper
	void open_samples_file(u32 counter, bool can_fail);
};

#endif /* OPROFPP_H */
