/**
 * @file string_manip.h
 * std::string helpers
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 * @author John Levon
 */

#ifndef STRING_MANIP_H
#define STRING_MANIP_H

#include <string>
#include <vector>

/**
 * @param str string
 * @param ch the char from where we erase character
 *
 * erase char from the last occurence of ch to the end of str and return
 * the string
 */
std::string erase_from_last_of(std::string const & str, char ch);

/**
 * @param str string
 * @param ch the characterto search
 *
 * erase char from the begin of str to the last
 * occurence of ch from and return the string
 */
std::string erase_to_last_of(std::string const & str, char ch);


/// conversion to std::string
std::string tostr(unsigned int i);

/**
 * @param result where to put results
 * @param str the string to tokenize
 * @param sep the separator_char
 *
 * separate fild in a string in a list of token; field are
 * separated by the sep character, sep char can be escaped
 * by '\' to specify a sep char in a token, '\' not followed
 * by a sep is taken as it e.g. "\,\a" --> ",\a"
 */
void separate_token(std::vector<std::string> & result, std::string const & str,
		    char sep);

/**
 * sample_filename - build a sample filename
 * @param  sample_dir sample files directory
 * @param sample_filename base name of sample file
 * @param counter counter nr
 *
 * If sample_dir is empty return sample_filename + "#" + counter
 * else return sample_dir + "/" + sample_filename + "#" + counter
 *
 * Existence of the samples files is not checked.
 */
std::string sample_filename(std::string const& sample_dir,
			    std::string const& sample_filename, int counter);

/// remove space and tabulation at left of input string return the new string
std::string ltrim(std::string const & str);
/// remove space and tabulation at right of input string, return the new string
std::string rtrim(std::string const & str);
/// return ltrim(rtrim(str)
std::string trim(std::string const & str);

#endif /* !STRING_MANIP_H */
