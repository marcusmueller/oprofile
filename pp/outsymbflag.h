/**
 * @file outsymbflag.h
 * Output options
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon <moz@compsoc.man.ac.uk>
 * @author Philippe Elie <phil_el@wanadoo.fr>
 */

#ifndef OUTSYMBFLAG_H
#define OUTSYMBFLAG_H

// FIXME

/**
 * flags passed to the ctr of an output_symbol object. This also specify the
 * order of field output: lower enum tag ==> comes first in output order
 * Note than changing value of enum is not safe. See output_symbol and
 * the osf_repeat_mask use. So you can't reorder easily the output order
 * of field
 */
enum outsymbflag {
       osf_none = 0,
       osf_vma = 1 << 0,
       /// this four field can be repeated on output for each counter.
       osf_nr_samples = 1 << 1,
       osf_nr_samples_cumulated = 1 << 2,
       osf_percent = 1 << 3,
       osf_percent_cumulated = 1 << 4,
       /// used internally
       osf_repeat_mask = osf_nr_samples + osf_nr_samples_cumulated +
                         osf_percent + osf_percent_cumulated,
       osf_symb_name = 1 << 5,
       osf_linenr_info = 1 << 6,
       osf_short_linenr_info = 1 << 7,// w/o path name
       osf_image_name = 1 << 8,
       osf_short_image_name = 1 << 9, // w/o path name
       /// only this field are showed in symbols samples details.
	osf_details = 1 << 10,		// for oprofpp -L / -s
	/// only this field are showed in symbols samples details.
	osf_details_mask = osf_nr_samples + osf_nr_samples_cumulated +
			osf_percent + osf_percent_cumulated +
			osf_vma + osf_linenr_info + osf_short_linenr_info,
	osf_show_all_counters = 1 << 11,// for oprofpp -L
	// special format modifier: output all field selected separated by a
	// single space. This is intended to run post profile tools as
	// front-end of other tools which treat samples information.
	osf_header = 1 << 12
};

#endif // OUTSYMBFLAG_H
