/* COPYRIGHT (C) 2002 Philippe Elie
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <iostream>

#include <qfiledialog.h>
#include <qtabwidget.h>
#include <qlayout.h>

#include "../pp/opf_filter.h"
#include "../util/file_manip.h"
#include "oprof_report.h"
#include "oprofpp_view.h"
#include "hotspot_view.h"

using std::string;
using std::cerr;

/**
 *  oprof_report - Constructs a oprof_report
 */
oprof_report::oprof_report()
	:
	oprof_report_base(0, 0, false, 0),
	oprofpp_view(new OprofppView(oprofpp_view_widget)),
	hotspot_view(new HotspotView(hotspot_tab)),
	samples_files(0)
{
	hotspot_tabLayout->addWidget(hotspot_view, 0, 0, 0);
}

/**
 *  ~oprof_report - Destroys the object and frees any allocated resources
 */
oprof_report::~oprof_report()
{
	delete samples_files;
}

/**
 * load_samples_files - load the user selected samples file.
 */ 
void oprof_report::load_samples_files(const string & filename)
{
	string lib_name;

	string temp_filename = strip_filename_suffix(filename);
	string app_name = extract_app_name(basename(temp_filename), lib_name);
	if (lib_name.length())
		app_name = lib_name;

	/* TODO: on which counter we want to work must be user selectable.
	 * for now let's as it but do not worry me about zero samples
	 * details bug. */
	int counter = -1;

	/* TODO: oprofpp_util.cpp, opf_container.cpp: handle all error
	 * through exception */
	try {
		opp_samples_files samples_file(temp_filename, counter);

		opp_bfd abfd(samples_file, demangle_filename(app_name));

		// we defer clearing the view after ensuring than nothing
		// bad occur (through exception) in file loading.
		mark_all_view_changed();

		delete samples_files;
		samples_files =
			new samples_files_t(true, osf_details, true, counter);

		// we filter nothing here to allow changing filtering later
		// w/o reloading the whole samples files (TODO)
		samples_files->add(samples_file, abfd);
	}

	/* ... TODO and handle the relevant exception here ... */
	catch (...) {
		// TODO QMessageBox ...
		cerr << "unknown exception ..." << endl;
	}
}

/**
 * mark_all_view_changed - mark all view has changed
 * allowing lazilly clear and rebuild
 */
void oprof_report::mark_all_view_changed()
{
	oprofpp_view->data_destroy();
	hotspot_view->data_destroy();
}

/**
 * do_open_file_or_dir - open file/directory
 * @base_dir: directory to start at
 * @dir_only: directory or filename to select
 *
 * Select a file or directory. The selection is returned;
 * an empty string if the selection was cancelled.
 * TODO: this must become a samples files folder
 */
static string const do_open_file_or_dir(string const & base_dir, bool dir_only)
{
	QString result;

	if (dir_only) {
		result = QFileDialog::getExistingDirectory(base_dir.c_str(), 0,
			"open_file_or_dir", "Get directory name", true);
	} else {
		result = QFileDialog::getOpenFileName(base_dir.c_str(), 0, 0,
			"open_file_or_dir", "Get filename");
	}

	if (result.isNull())
		return string();
	else
		return result.latin1();
}

/**
 * on_open - event handler 
 *
 * get user input to select a new samples file.
 */
void oprof_report::on_open()
{
	// TODO: configuration string (must we share config with oprof_start ?)
	static string dirname = "/var/opd/samples/*";
	string filename = do_open_file_or_dir(dirname, false);
	if (filename.length()) {
		load_samples_files(filename);

		// ensure refresh of the current view.
		on_tab_change(tab_report->currentPage());
	}
}

/**
 * on_tab_change - event handler 
 *
 * invoked when the user select a different view
 */
void oprof_report::on_tab_change(QWidget* new_tab)
{
	if (samples_files) {
		// for now I handle this through string compare, if nr of view
		// becomes a little what to great handle it through a
		// std::map<QWidget*, OpView*> associative array filled in ctor
		if (new_tab->name() == QString("oprofpp_tab")) {
			oprofpp_view->data_change(samples_files);
		} else if (new_tab->name() == QString("hotspot_tab")) {
			hotspot_view->data_change(samples_files);
		}
	}
}
