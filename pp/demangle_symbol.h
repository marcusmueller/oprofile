/**
 * @file demangle_symbol.h
 * Demangle a C++ symbol
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 * 
 * @author John Levon <moz@compsoc.man.ac.uk>
 */

#ifndef DEMANGLE_SYMBOL_H
#define DEMANGLE_SYMBOL_H
 
#include <string>
 
/**
 * demangle_symbol - demangle a symbol
 * @param name the mangled symbol name
 * @return the demangled name
 *
 * Demangle the symbol name, if the global
 * variable demangle is true.
 *
 * The demangled name lists the parameters and type
 * qualifiers such as "const".
 */
std::string const demangle_symbol(std::string const & name);

#endif // DEMANGLE_SYMBOL_H
