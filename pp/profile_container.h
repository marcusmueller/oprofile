/**
 * @file profile_container.h
 * Container associating symbols and samples
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 * @author John Levon
 */

#ifndef PROFILE_CONTAINER_H
#define PROFILE_CONTAINER_H

#include <string>
#include <vector>

#include "opp_symbol.h"
#include "outsymbflag.h"
#include "utility.h"
#include "op_bfd.h"

class sample_container_imp_t;
class symbol_container_imp_t;
class profile_t;

/** store multiple samples files belonging to the same profling session.
 * So on can hold samples files for arbitrary counter and binary image */
class profile_container_t /*:*/ noncopyable {
public:
	/**
	 * Build an object to store information on samples. All parameters
	 * acts as hint for what you will request after recording samples and
	 * so on allow optimizations during recording the information.
	 * @param add_zero_samples_symbols must we add to the symbol container
	 * symbols with zero samples count
	 * @param flags optimize hint to add samples. The flags is a promise
	 * on what will be required as information in future. Avoid to pass
	 * osf_linenr_info greatly improve performance of add. Avoiding
	 * osf_details is also an improvement.
	 * @param counter_mask which counter we must record
	 */
	profile_container_t(bool add_zero_samples_symbols, outsymbflag flags,
			     int counter_mask);

	~profile_container_t();
 
	/**
	 * add() -  record symbols/samples in the underlined container
	 * @param profile the samples files container
	 * @param abfd the associated bfd object
	 * @param symbol_name if non empty add will record samples only
	 * for this symbol name else all samples will be recorded
	 *
	 * add() is an helper for delayed ctor. Take care you can't safely
	 * make any call to add after any other member function call.
	 * Obviously you can add only samples files which are coherent (same
	 * sampling rate, same events etc.)
	 */
	void add(profile_t const & profile, op_bfd const & abfd,
		 std::string const & symbol_name = std::string());

	/// Find a symbol from its vma, return zero if no symbol at this vma
	symbol_entry const * find_symbol(bfd_vma vma) const;

	/// Find a symbol from its name, return zero if no symbol found
	symbol_entry const * find_symbol(std::string const & name) const;

	/// Find a symbol from its filename, linenr, return zero if no symbol
	/// at this location
	symbol_entry const * find_symbol(std::string const & filename,
					size_t linenr) const;

	/// Find a sample by its vma, return zero if no sample at this vma
	sample_entry const * find_sample(bfd_vma vma) const;

	/// Return a sample_entry by its index, index must be valid
	sample_entry const & get_samples(sample_index_t idx) const;

	/// a collection of sorted symbols
	typedef std::vector<symbol_entry const *> symbol_collection;

	/**
	 * select_symbols - create a set of symbols sorted by sample count
	 * @param ctr on what counter sorting must be made and threshold
	 *   selection must be made
	 * @param threshold select symbols which contains more than
	 *   threshold percent of samples
	 * @param until_threshold rather to get symbols with more than
	 *   percent threshold samples select symbols until the cumulated
	 *   count of samples reach threshold percent
	 * @param sort_by_vma sort symbols by vma not counter samples
	 * @return a sorted vector of symbols
	 *
	 * until_threshold and threshold acts like the -w and -u options
	 * of op_to_source. If you need to get all symbols call it with
	 * threshold == 0.0 and !until_threshold
	 */
	symbol_collection const select_symbols(
		size_t ctr, double threshold,
		bool until_threshold,
		bool sort_by_vma = false) const;

	/// Like select_symbols for filename without allowing sort by vma.
	std::vector<std::string> const select_filename(size_t ctr,
		double threshold, bool until_threshold) const;

	/// return the total number of samples for counter_nr
	u32 samples_count(size_t counter_nr) const;

	/// Get the samples count which belongs to filename. Return false if
	/// no samples found.
	bool samples_count(counter_array_t & result,
			   std::string const & filename) const;
	/// Get the samples count which belongs to filename, linenr. Return
	/// false if no samples found.
	bool samples_count(counter_array_t & result,
			   std::string const & filename,
			   size_t linenr) const;
	/// you can call this *after* the first call to add() else the
	/// application exit(1) with a meaningfull error message
	uint get_nr_counters() const;

private:
	/// helper for do_add()
	void add_samples(profile_t const & profile,
			 op_bfd const & abfd, symbol_index_t sym_index,
			 u32 start, u32 end, bfd_vma base_vma,
			 std::string const & image_name);

	/**
	 * create an unique artificial symbol for an offset range. The range
	 * is only a hint of the maximum size of the created symbol. We
	 * give to the symbol an unique name as ?image_file_name#order and
	 * a range up to the nearest of syms or for the whole range if no
	 * syms exist after the start offset. the end parameter is updated
	 * to reflect the symbol range.
	 *
	 * The rationale here is to try to create symbols for alignment between
	 * function as little as possible and to create meaningfull symbols
	 * for special case such image w/o symbol.
	 */
	std::string create_artificial_symbol(op_bfd const & abfd, u32 start,
					     u32 & end, size_t & order);

	/// The symbols collected by oprofpp sorted by increased vma, provide
	/// also a sort order on samples count for each counter.
	scoped_ptr<symbol_container_imp_t> symbols;
	/// The samples count collected by oprofpp sorted by increased vma,
	/// provide also a sort order on (filename, linenr)
	scoped_ptr<sample_container_imp_t> samples;
	/// build() must count samples count for each counter so cache it here
	/// since user of profile_container_t often need it later.
	counter_array_t counter;
	/// maximum number of counter available
	uint nr_counters;

	/// parameters passed to ctor
	bool add_zero_samples_symbols;
	outsymbflag flags;
	int counter_mask;
};

/**
 * add_samples - populate a samples container with samples
 * @param samples the samples container to populate
 * @param sample_filename samples filename
 * @param counter_mask the counter nr mask each bit at pos i on mean open
 *  this samples files nr i
 * @param binary_name the name of the binary image
 * @param excluded_symbols a vector of symbol name to ignore
 * @param symbol if non empty record only samples for this symbol
 *
 * open a bfd object getting symbols name, then populate samples with the
 * relevant samples
 */
bool add_samples(profile_container_t & samples, std::string sample_filename,
		 size_t counter_mask, std::string binary_name,
		 std::vector<std::string> const & excluded_symbols =
		 	std::vector<std::string>(),
		 std::string symbol = std::string());

#endif /* !PROFILE_CONTAINER_H */
