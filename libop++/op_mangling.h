/**
 * @file op_mangling.h
 * Mangling and unmangling of sample filenames
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon <moz@compsoc.man.ac.uk>
 * @author Philippe Elie <phil_el@wanadoo.fr>
 */

#ifndef OP_MANGLING_H
#define OP_MANGLING_H
 
#include <string>
 
/**
 * remangle - convert a filename into the related sample file name
 * @param filename the filename string
 *
 * The returned filename is still relative to the samples directory.
 */
std::string remangle_filename(std::string const & filename);

/**
 * demangle_filename - convert a sample filenames into the related
 * image file name
 * @param samples_filename the samples image filename
 *
 * if samples_filename does not contain any %OPD_MANGLE_CHAR
 * the string samples_filename itself is returned.
 */
std::string demangle_filename(std::string const & samples_filename);

#endif // OP_MANGLING_H
