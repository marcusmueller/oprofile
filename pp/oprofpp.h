/**
 * @file oprofpp.h
 * Main post-profiling tool
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 * 
 * @author John Levon <moz@compsoc.man.ac.uk>
 * @author Philippe Elie <phil_el@wanadoo.fr>
 */

#ifndef OPROFPP_H
#define OPROFPP_H

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
#include "op_sample_file.h"
#include "op_hw_config.h"
#include "op_bfd.h"
#include "samples_file.h"

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
void verbprintf(char const * args, ...) OP_VERBPRINTF_FORMAT;

 
/**
 * process command line options
 * @param filename a filename passed on the command line, can be NULL
 * @param image_file where to store the image file name
 * @param sample_file ditto for sample filename
 * @param counter where to put the counter command line argument
 * @param sort_by_counter counter nr used for sort purpose
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
void opp_treat_options(std::string const & filename, 
		       std::string & image_file, std::string & sample_file,
		       int & counter, int & sort_by_counter);

/**
 * quit with error
 * @param err error to show
 *
 * err may be NULL
 */
void quit_error(char const *err);

/**
 * remangle - convert a filename into the related sample file name
 * @param filename the filename string
 */
std::string remangle(std::string const & filename);

/**
 * convert a sample filenames into the related image file name
 * @param samples_filename the samples image filename
 *
 * if samples_filename does not contain any %OPD_MANGLE_CHAR
 * the string samples_filename itself is returned.
 */
std::string demangle_filename(std::string const & samples_filename);

/**
 * check if the symbol is in the exclude list
 * @param symbol symbol name to check
 *
 * return true if symbol is in the list of excluded symbol
 */
bool is_excluded_symbol(std::string const & symbol);

/**
 * sanity check of a struct opd_header *
 * @param header a pointer to header to check
 *
 * all error are fatal
 */
void check_event(opd_header const * header);

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
uint counter_mask(std::string const &);

/**
 * @param samples the samples files
 * @param image_name the image filename
 *
 * check than the modification time of image_name is the same as provided
 * in the samples file header
 */
void check_mtime(opp_samples_files const & samples, std::string image_name);

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
	counter_array_t & operator+=(counter_array_t const & rhs);

private:
	u32 value[OP_MAX_COUNTERS];
};

#endif /* OPROFPP_H */
