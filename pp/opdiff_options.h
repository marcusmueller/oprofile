/**
 * @file opdiff_options.h
 * Options for opdiff tool
 *
 * @remark Copyright 2002, 2003 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 */

#ifndef OPDIFF_OPTIONS_H
#define OPDIFF_OPTIONS_H

#include "common_option.h"

namespace options {
}

/**
 * handle_options - process command line
 * @param non_options vector of non options string
 *
 * Process the arguments, fatally complaining on error.
 */
void handle_options(std::vector<std::string> const & non_options);

#endif // OPDIFF_OPTIONS_H
