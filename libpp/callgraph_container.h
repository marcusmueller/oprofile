/**
 * @file callgraph_container.h
 * Container associating symbols and caller/caller symbols
 *
 * @remark Copyright 2004 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 * @author John Levon
 */

#ifndef CALLGRAPH_CONTAINER_H
#define CALLGRAPH_CONTAINER_H

#include <set>
#include <vector>
#include <list>
#include <string>

#include "bfd.h"
#include "profile_container.h"
#include "symbol.h"
#include "symbol_functors.h"

class inverted_profile;
class extra_images;

/**
 * Yes, this looks like weird since we store callee_counts w/o knowing the
 * callee. It's designed in this way because it allows symmetric map in
 * arc_recorder. Mis-use is protected by the arc_recorder interface.
 */
struct cg_symbol : public symbol_entry {
	cg_symbol(symbol_entry const & symb) : symbol_entry(symb) {}
	cg_symbol() {}
	count_array_t self_counts;
	count_array_t callee_counts;
};


/**
 * During building a callgraph_container we store all caller/callee
 * relationship in this container.
 *
 * An "arc" is simply a description of a call from one function to
 * another.
 */
class arc_recorder {
public:
	~arc_recorder();

	void add_arc(cg_symbol const & caller, cg_symbol const * callee);

	/**
	 * Finalize the recording after all arcs have been added
	 * to propagate callee counts.
	 */
	void fixup_callee_counts();

	// sorted sequence of cg_symbol.
	std::vector<cg_symbol> get_arc() const;
	std::vector<cg_symbol> get_callee(cg_symbol const &) const;
	std::vector<cg_symbol> get_caller(cg_symbol const &) const;

private:
	cg_symbol const * find_caller(cg_symbol const &) const;

	typedef std::multimap<cg_symbol, cg_symbol const *, less_symbol> map_t;
	typedef map_t::const_iterator iterator;
	map_t caller_callee;
	map_t callee_caller;
};


/**
 * FIXME
 */
class callgraph_container {
public:
	/**
	 * Populate the container, must be called once only.
	 * @param iprofiles  sample file list including callgraph files.
	 * @param extra  extra image list to fixup binary name.
	 *
	 * Currently all errors core dump.
	 * FIXME: consider if this should be a ctor
	 */
	void populate(std::list<inverted_profile> const & iprofiles,
		      extra_images const & extra);

	/// These just dispatch to arc_recorder. It's the way client
	/// code acquires results.
	std::vector<cg_symbol> get_arc() const;
	std::vector<cg_symbol> get_callee(cg_symbol const &) const;
	std::vector<cg_symbol> get_caller(cg_symbol const &) const;

private:
	/**
	 * Record caller/callee for one cg file
	 * @param profile  one callgraph file stored in a profile_t
	 * @param caller_bfd  the caller bfd
	 * @param bfd_caller_ok  true if we succefully open the binary
	 * @param callee_bfd  the callee bfd
	 * @param app_name  the owning application
	 * @param symbols  the profile_container holding all non cg samples.
	 */
	void add(profile_t const & profile, op_bfd const & caller_bfd,
	         bool bfd_caller_ok, op_bfd const & callee_bfd,
		 std::string const & app_name,
	         profile_container const & symbols);

	/// add fake arc <from, NULL> to record leaf symbols.
	void add_leaf_arc(profile_container const & symbols);

	/// A structured representation of the callgraph.
	arc_recorder recorder;
};

#endif /* !CALLGRAPH_CONTAINER_H */
