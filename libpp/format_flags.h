/**
 * @file format_flags.h
 * output options
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 */

#ifndef FORMAT_FLAGS_H
#define FORMAT_FLAGS_H

/**
 * flags passed to the ctor of an output_symbol object. This also specify the
 * order of field output: lower enum tag ==> comes first in output order.
 * Note than changing value of enum is not safe. (why not ?)
 *
 * \sa formatter
 */
enum format_flags {
	ff_none = 0,
	/// a formatted memory address
	ff_vma = 1 << 0,
	/// number of samples
	ff_nr_samples = 1 << 1,
	/// number of samples accumulated
	ff_nr_samples_cumulated = 1 << 2,
	/// relative percentage of samples
	ff_percent = 1 << 3,
	/// relative percentage of samples accumulated
	ff_percent_cumulated = 1 << 4,
	/// output debug filename and line nr.
	ff_linenr_info = 1 << 5,
	/// output the image name for this line
	ff_image_name = 1 << 6,
	/// output owning application name
	ff_app_name = 1 << 7,
	/// output the (demangled) symbol name
	ff_symb_name = 1 << 8,
	/**
	 * Output percentage for details, not relative
	 * to symbol but relative to the total nr of samples
	 */
	ff_percent_details = 1 << 9,
	/**
	 * Output percentage for details, not relative
	 * to symbol but relative to the total nr of samples,
	 * accumulated
	 */
	ff_percent_cumulated_details = 1 << 10,

	/// These fields are considered immutable when showing details for one
	/// symbol, we show them only when outputting the symbol itself but
	/// we don't display them in the details output line
	ff_immutable_field = ff_symb_name + ff_image_name + ff_app_name
};


/**
 * General hints about formatting of the columnar output.
 */
enum column_flags {
	cf_none = 0,
	cf_multiple_apps = 1 << 0,
	cf_64bit_vma = 1 << 1,
	cf_image_name = 1 << 2
};

#endif // FORMAT_FLAGS_H
