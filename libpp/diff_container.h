/**
 * @file diff_container.h
 * Container for diffed symbols
 *
 * @remark Copyright 2005 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 * @author John Levon
 */

#ifndef DIFF_CONTAINER_H
#define DIFF_CONTAINER_H

#include "profile_container.h"
#include "symbol.h"

/**
 * Store two profiles for diffing.
 */
class diff_container : noncopyable {
public:
	/// populate the collection of diffed symbols
	diff_container(profile_container const & pc1,
	               profile_container const & pc2);

	~diff_container() {}
 
	/// return a collection of diffed symbols
	diff_collection const get_symbols() const;

	/// total count for 'new' profile
	count_array_t const samples_count() const;

private:
	/// the diffed symbols
	diff_collection syms;

	/// samples count for pc1
	count_array_t total1;

	/// samples count for pc2
	count_array_t total2;
};

#endif /* !DIFF_CONTAINER_H */
