/**
 * @file oprof_report.cpp
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 */

#include <iostream>

#include <qfiledialog.h>
#include <qtabwidget.h>
#include <qlayout.h>

#include "../pp/samples_container.h"
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
	samples_container(0)
{
	hotspot_tabLayout->addWidget(hotspot_view, 0, 0, 0);
}

/**
 *  ~oprof_report - Destroys the object and frees any allocated resources
 */
oprof_report::~oprof_report()
{
	delete samples_container;
}

/**
 * load_samples_files - load the user selected samples file.
 */
void oprof_report::load_samples_files(string const & filename)
{
	string lib_name;

	string temp_filename = strip_filename_suffix(filename);
	string app_name = extract_app_name(basename(temp_filename), lib_name);
	if (lib_name.length())
		app_name = lib_name;

	/* FIXME: on which counter we want to work must be user selectable.
	 * for now let's as it but do not worry me about zero samples
	 * details bug. */
	int counter = -1;

	/* FIXME: oprofpp_util.cpp, opf_container.cpp: handle all error
	 * through exception */
	try {
		opp_samples_files samples_file(temp_filename, counter);

		opp_bfd abfd(samples_file, demangle_filename(app_name));

		// we defer clearing the view after ensuring than nothing
		// bad occur (through exception) in file loading.
		mark_all_view_changed();

		delete samples_container;
		samples_container = new samples_container_t(true, osf_details,
							    true, counter);

		// we filter nothing here to allow changing filtering later
		// w/o reloading the whole samples files (FIXME)
		samples_container->add(samples_file, abfd);
	}

	/* ... FIXME and handle the relevant exception here ... */
	catch (...) {
		// FIXME QMessageBox ...
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
 * @param base_dir  directory to start at
 * @param dir_only  directory or filename to select
 *
 * Select a file or directory. The selection is returned;
 * an empty string if the selection was cancelled.
 * FIXME: this must become a samples files folder
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
	static string dirname = OP_SAMPLES_DIR "*";
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
	if (samples_container) {
		// for now I handle this through string compare, if nr of view
		// becomes a little what to great handle it through a
		// std::map<QWidget*, OpView*> associative array filled in ctor
		if (new_tab->name() == QString("oprofpp_tab")) {
			oprofpp_view->data_change(samples_container);
		} else if (new_tab->name() == QString("hotspot_tab")) {
			hotspot_view->data_change(samples_container);
		}
	}
}
