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

#ifndef OPROF_REPORT_H
#define OPROF_REPORT_H

#include <string>

#include "ui/oprof_report.base.h"

class samples_files_t;
class OprofppView;
class OprofppHotspot;

/**
 * oprofpp_report - the main class of the oprof_report application.
 *  this represent a main window with an imbedded tabbed widget wich
 * contains different view of data on each tab. When loading a new samples
 * files view are destroyed and are re-created lazzily when the relevant tab
 * is selected for the first time. This is mainly to allow to add as many
 * view we want w/o getting excessive cpu time waste.
 *
 * the design is basic, view are handled through separate class derived from
 * from op_view. see op_view.h.
 */
/* TODO: right click on view dispatching it to the relevant view */
class oprof_report : public oprof_report_base
{
	Q_OBJECT

public:
	oprof_report();
	~oprof_report();
    
protected slots:
	/// open a new samples file
	void on_open();
	/// used to lazilly create the view.
	void on_tab_change(QWidget*);

private:
	/// helper for on_open, must never throw
	void load_samples_files(const std::string & filename);

	/// clear and mark all view as destroyed
	void destroy_all_view();

	/// the oprofpp view handling
	OprofppView * oprofpp_view;

	/// the (feel to write it) hotspot view handling
	OprofppHotspot* hotspot_view;

	samples_files_t * samples_files;
};

#endif // OPROF_REPORT_H
