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
#include "format_flags.h"

class inverted_profile;
class extra_images;
class image_set;

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

typedef std::vector<cg_symbol> cg_collection;


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
	 * @param threshold  ignore arc below this threshold
	 * @param totals  cumulated counts of leaf node
	 *
	 * Finalize the recording after all arcs have been added to propagate
	 * callee counts then remove all leaf node not statisfying threshold
	 * and propagate the removed node to parent arc.
	 */
	void fixup_callee_counts(double threshold, count_array_t & totals);

	// sorted sequence of cg_symbol.
	cg_collection get_arc() const;
	cg_collection get_callee(cg_symbol const &) const;
	cg_collection get_caller(cg_symbol const &) const;

private:
	typedef std::multimap<cg_symbol, cg_symbol const *, less_symbol> map_t;
	typedef map_t::const_iterator iterator;

	cg_symbol const * find_caller(cg_symbol const &) const;

	/**
	 * @param percent  threshold criteria in percent
	 * @param totals  cumulated counts of leaf node..
	 *
	 * return a vector of iterator to the caller_callee object leaf node
	 * *not* statisfying the give threshold, totals counts is updated
	 * in prevision of the removal of these nodes
	 */
	std::vector<map_t::iterator> select_leaf(double percent,
		count_array_t & totals);

	/// remove this cg_symbol from caller_callee map .second member and
	/// from the callee_caller map, used to update the removal of a leaf
	/// node from the caller_callee map.
	void remove(cg_symbol const & caller);

	/// returned iterator point into the caller_callee map
	iterator find_arc(cg_symbol const &, cg_symbol const &);

	map_t caller_callee;
	map_t callee_caller;
};


/**
 * Store all callgraph information for the given profiles
 */
class callgraph_container {
public:
	/**
	 * Populate the container, must be called once only.
	 * @param archive_path  oparchive prefix path
	 * @param iprofiles  sample file list including callgraph files.
	 * @param extra  extra image list to fixup binary name.
	 * @param debug_info  true if we must record linenr information
	 * @param threshold  ignore sample percent below this threshold
	 * @param merge_lib  merge library samples
	 *
	 * Currently all errors core dump.
	 * FIXME: consider if this should be a ctor
	 */
	void populate(std::string const & archive_path,
		      std::list<inverted_profile> const & iprofiles,
		      extra_images const & extra, bool debug_info,
		      double threshold, bool merge_lib);

	/// return hint on how data must be displayed.
	column_flags output_hint() const;

	/// return the total number of samples.
	count_array_t samples_count() const;

	/// These just dispatch to arc_recorder. It's the way client
	/// code acquires results.
	cg_collection get_arc() const;
	cg_collection get_callee(cg_symbol const &) const;
	cg_collection get_caller(cg_symbol const &) const;

private:
	/**
	 * Record caller/callee for one cg file
	 * @param profile  one callgraph file stored in a profile_t
	 * @param caller_bfd  the caller bfd
	 * @param bfd_caller_ok  true if we succefully open the binary
	 * @param callee_bfd  the callee bfd
	 * @param app_name  the owning application
	 * @param symbols  the profile_container holding all non cg samples.
	 * @param debug_info  record linenr debug information
	 * @param pclass  profile class nr
	 */
	void add(profile_t const & profile, op_bfd const & caller_bfd,
	         bool bfd_caller_ok, op_bfd const & callee_bfd,
		 std::string const & app_name, 
		 profile_container const & symbols, bool debug_info,
		 size_t pclass);

	void populate(std::string const & archive_path,
		      std::list<image_set> const & lset,
		      std::string const & app_image,
		      extra_images const & extra, size_t pclass,
		      profile_container const & symbols, bool debug_info,
		      bool merge_lib);
	void populate(std::string const & archive_path,
		      std::list<std::string> const & cg_files,
		      std::string const & app_image,
		      extra_images const & extra, size_t pclass,
		      profile_container const & symbols, bool debug_info,
		      bool merge_lib);

	/// add fake arc <from, NULL> to record leaf symbols.
	void add_leaf_arc(profile_container const & symbols);

	/// Chached value of samples count.
	count_array_t total_count;

	/// A structured representation of the callgraph.
	arc_recorder recorder;
};

#endif /* !CALLGRAPH_CONTAINER_H */
