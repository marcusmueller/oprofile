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
class notcopyable {
protected:
	notcopyable() {}
	~notcopyable() {}
private:
        notcopyable(notcopyable const &);
        const notcopyable& operator=(notcopyable const &);
};

#endif /* !UTILITY_H */
