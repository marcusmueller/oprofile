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

/**
 * Yes this look like weird since we store a callee_counts w/o knowning the
 * callee. It's designed in this way because it allows symetric map in
 * callee_caller_recorder, cg_symbol can be get only through public interface
 * of callee_caller_recorder which hide potential misuse of this struct.
 */
struct cg_symbol : public symbol_entry {
	cg_symbol(symbol_entry const & symb) : symbol_entry(symb) {}
	cg_symbol() {}
	count_array_t self_counts;
	count_array_t callee_counts;
};


/// During building a callgraph_container we store all caller/callee
/// relationship in this container.
class caller_callee_recorder {
public:
	~caller_callee_recorder();

	void add_arc(cg_symbol const & caller, cg_symbol const * callee);

	/// Finalize the recording after all arc has been added to propagate
	/// callee counts.
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


class callgraph_container {
public:
	/**
	 * populate the container, must be called one time only.
	 * @param iprofiles  sample file list including callgraph files.
	 *
	 * Currently all error core dump.
	 */
	void populate(std::list<inverted_profile> const & iprofiles);

	/// These just dispatch to callee_caller_recorder. It's the way client
	/// code acquire results.
	std::vector<cg_symbol> get_arc() const;
	std::vector<cg_symbol> get_callee(cg_symbol const &) const;
	std::vector<cg_symbol> get_caller(cg_symbol const &) const;

private:
	/** Record caller/callee for one cg file
	 * @param profile  one callgraph file stored in a profile_t
	 * @param bfd_caller  the caller bfd
	 * @param bfd_callee  the callee bfd
	 * @param app_name  the owning application
	 * @param symbols  the profile_container holding all non cg samples.
	 */
	void add(profile_t const & profile, op_bfd const & bfd_caller,
		 op_bfd const & bfd_callee, std::string const & app_name,
		 profile_container const & symbols);

	/// add fake arc <from, NULL> to record leaf symbols.
	void add_leaf_arc(profile_container const & symbols);

	/// A structured representation of the callgraph.
	caller_callee_recorder recorder;
};

#endif /* !CALLGRAPH_CONTAINER_H */
