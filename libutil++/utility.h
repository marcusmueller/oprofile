/**
 * @file utility.h
 * General purpose C++ utility
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie <phil_el@wanadoo.fr>
 * @author John Levon <moz@compsoc.man.ac.uk>
 */

#ifndef UTILITY_H
#define UTILITY_H

/** notcopyable : object of class derived from this class can't be copyed
 * neither copy-constructible */
/** FIXME: take care using this class, it seems a bug in gcc 2.91 forgive to
 * use it blindly in some case (I do not know in which circumstances this bug
 * appears). */
class noncopyable {
protected:
	noncopyable() {}
	~noncopyable() {}
private:
        noncopyable(noncopyable const &);
        noncopyable const & operator=(noncopyable const &);
};

/// work round for 2.91, this means you can't derive from noncopyable
/// in the usual way but rather you need to struct derived /*:*/ noncpyable
#if __GNUC__ == 2 && __GNUC_MINOR__ == 91
#define noncopyable
#else
#define noncopyable : noncopyable
#endif

// the class noncopyable get this copyright :
//  (C) Copyright boost.org 1999. Permission to copy, use, modify, sell
//  and distribute this software is granted provided this copyright
//  notice appears in all copies. This software is provided "as is" without
//  express or implied warranty, and with no claim as to its suitability for
//  any purpose.

#endif /* !UTILITY_H */
