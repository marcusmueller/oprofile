/**
 * @file partition_files.cpp
 * Encapsulation for merging and partitioning samples filename list
 *
 * @remark Copyright 2003 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 */

#include <set>
#include <algorithm>
#include <iterator>

#include "cverb.h"
#include "file_manip.h"
#include "partition_files.h"
#include "split_sample_filename.h"

using namespace std;

unmergeable_profile::unmergeable_profile(std::string const & event_,
					 std::string const & count_)
	:
	event(event_),
	count(count_)
{
}


bool unmergeable_profile::operator<(unmergeable_profile const & rhs) const
{
	return event < rhs.event || (event == rhs.event && count < rhs.count);
}


ostream & operator<<(ostream & out, unmergeable_profile const & lhs)
{
	out << lhs.event << " " << lhs.count;
	return out;
}


vector<unmergeable_profile> merge_profile(list<string> const & files)
{
	if (files.empty())
		return vector<unmergeable_profile>();

	set<unmergeable_profile> spec_set;

	// FIXME: what is this for ?
	split_sample_filename model = split_sample_file(*files.begin());

	list<string>::const_iterator it;
	for (it = files.begin(); it != files.end(); ++it) {
		split_sample_filename spec = split_sample_file(*it);
		spec_set.insert(unmergeable_profile(spec.event, spec.count));
	}

	vector<unmergeable_profile> result;
	copy(spec_set.begin(), spec_set.end(), back_inserter(result));

	return result;
}


unmergeable_samplefile
unmerge_samplefile(list<string> const & files,
	vector<unmergeable_profile> const & profiles)
{
	unmergeable_samplefile result(profiles.size());

	// FIXME: inneficient, a multiset would do the trick in a better way
	// but for now I want to test synched walking through multiple profile
	unmergeable_samplefile::iterator result_it = result.begin();
	vector<unmergeable_profile>::const_iterator const cend =
		profiles.end();
	vector<unmergeable_profile>::const_iterator cit = profiles.begin();
	for ( ; cit != cend ; ++cit, ++result_it) {
		list<string>::const_iterator const files_cend = files.end();
		list<string>::const_iterator files_cit = files.begin();
		for ( ; files_cit != files_cend; ++files_cit) {
			// FIXME: this is redudant with partition_files, this
			// mean we would use split_sample_file rather than
			// string earlier and we would get a
			// list<split_sample_filename> const & rather than a
			// list<string> const & files parameter
			// or we want to ret vector<list<split_sample_filename)
			split_sample_filename splitted =
				split_sample_file(*files_cit);

			if (cit->event == splitted.event &&
			    cit->count == splitted.count) {
				result_it->push_back(*files_cit);
			}
		}
		
	}

	return result;
}


/**
 * merge_compare - comparator used to partition a set of samples filename
 * into equivalence class.  The equivalence relation equiv(a, b) is given by
 * !merge_compare(a, b) && !merge_compare(b, a)
 */
class merge_compare {
public:
	merge_compare(merge_option const & merge_by);
	bool operator()(split_sample_filename const & lhs,
			split_sample_filename const & rhs) const;
private:
	merge_option merge_by;
};


merge_compare::merge_compare(merge_option const & merge_by_)
	:
	merge_by(merge_by_)
{
}


bool merge_compare::operator()(split_sample_filename const & lhs,
			       split_sample_filename const & rhs) const
{
	if (merge_by.lib) {
		if (lhs.lib_image != rhs.lib_image)
			return lhs.lib_image < rhs.lib_image;
		if (lhs.lib_image.empty() && lhs.image != rhs.image)
			return lhs.image < rhs.image;
	} else {
		if (lhs.image != rhs.image)
			return lhs.image < rhs.image;
	}

	if (lhs.event != rhs.event)
		return lhs.event < rhs.event;

	if (lhs.count != rhs.count)
		return lhs.count < rhs.count;

	if (!merge_by.cpu && lhs.cpu != rhs.cpu)
		return lhs.cpu < rhs.cpu;

	if (!merge_by.tid && lhs.tid != rhs.tid)
		return lhs.tid < rhs.tid;

	if (!merge_by.tgid && lhs.tgid != rhs.tgid)
		return lhs.tgid < rhs.tgid;

	if (!merge_by.unitmask && lhs.unitmask != rhs.unitmask)
		return lhs.unitmask < rhs.unitmask;

	return false;
}


partition_files::partition_files(list<string> const & filename,
				 merge_option const & merge_by)
{
	typedef multiset<split_sample_filename, merge_compare> spec_set;

	merge_compare compare(merge_by);
	spec_set files(compare);

	list<string>::const_iterator cit;
	for (cit = filename.begin(); cit != filename.end(); ++cit)
		files.insert(split_sample_file(*cit));

	spec_set::const_iterator it = files.begin();
	while (it != files.end()) {
		pair<spec_set::const_iterator, spec_set::const_iterator>
			p_it = files.equal_range(*it);

		filename_set temp;
		copy(p_it.first, p_it.second, back_inserter(temp));
		filenames.push_back(temp);

		it = p_it.second;
	}

	cverb << "Partition entries: " << nr_set() << endl;
	filename_partition::const_iterator fit;
	for (fit = filenames.begin(); fit != filenames.end(); ++fit) {
		cverb << "Partition entry:\n";
		copy(fit->begin(), fit->end(), 
		     ostream_iterator<split_sample_filename>(cverb, ""));
	}

	// In some case a primary image can be dependent such:
	// {root}vmlinux and {root}/bin/bash/{dep}/{root}/vmlinux,
	// merge_compare() is unable to handle this properly so we must fix it

	if (!merge_by.lib) {
		return;
	}

	// FIXME: this would be handled in merge_compare() but, until we 'fix'
	// daemon to encode primary image as {root}/binary/{dep}/{root}/binary,
	// we can't.
	// FIXME O(nr_set()*nr_set())
	filename_partition::iterator fend = filenames.end();
	filename_partition::iterator cur;
	for (cur = filenames.begin(); cur != fend; ++cur) {
		filename_partition::iterator candidate = cur;
		for (++candidate; candidate != fend;) {
			string image_name;

			// assert(!cur->empty() && !candidate->empty())

			if (cur->begin()->image ==
			    candidate->begin()->lib_image) {
				image_name = cur->begin()->image;
			} else if (cur->begin()->lib_image ==
				   candidate->begin()->image) {
				image_name = candidate->begin()->image;
			}

			if (!image_name.empty()) {
				cur->splice(cur->end(), *candidate);
				candidate = filenames.erase(candidate);
			} else {
				++candidate;
			}
		}
	}
}


size_t partition_files::nr_set() const
{
	return filenames.size();
}


partition_files::filename_set const & partition_files::set(size_t index) const
{
	filename_partition::const_iterator it = filenames.begin();
	// cast because parameter to advance must be signed, using unsigned
	// produce warning ... (FIXME: use a vector of list ?)
	advance(it, filename_partition::difference_type(index));

	return *it;
}


namespace {

set<string> warned_images;

void not_found(string const & image)
{
	static bool warned_already;
	if (warned_images.find(image) == warned_images.end()) {
		cerr << "warning: couldn't find the binary file "
		     << image << endl;
		warned_images.insert(image);
	}

	if (!warned_already) {
		cerr << "Try adding a search path with the "
		     << "-p option." << endl;
		warned_already = true;
	}
}


void not_readable(string const & image)
{
	if (warned_images.find(image) == warned_images.end()) {
		cerr << "warning: couldn't read the binary file "
		     << image << endl;
		warned_images.insert(image);
	}
}


struct handle_insert {
	handle_insert(image_set & o, extra_images const & e)
		: out(o), extra(e) {}

	void operator()(split_sample_filename const & profile) {
		string const image_name = profile.lib_image.empty()
			? profile.image : profile.lib_image;

		string const found_name = find_image_path(image_name, extra);

		if (found_name.empty()) {
			not_found(image_name);
		} else if (!op_file_readable(found_name)) {
			not_readable(image_name);
		} else {
			image_set::value_type value(found_name, profile);
			out.insert(value);
		}
	}

private:
	image_set & out;
	extra_images const & extra;
};

}

		
image_set sort_by_image(partition_files const & files,
			extra_images const & extra_images)
{
	image_set result;

	for (size_t i = 0 ; i < files.nr_set(); ++i) {
		partition_files::filename_set const & file_set = files.set(i);

		for_each(file_set.begin(), file_set.end(),
		         handle_insert(result, extra_images));
	}

	return result;
}
