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
 * quit with error
 * @param err error to show
 *
 * err may be NULL
 */
void quit_error(char const *err);


/**
 * check if the symbol is in the exclude list
 * @param symbol symbol name to check
 *
 * return true if symbol is in the list of excluded symbol
 */
bool is_excluded_symbol(std::string const & symbol);

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

#endif /* OPROFPP_H */
