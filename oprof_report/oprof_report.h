/**
 * @file oprof_report.h
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie <phil_el@wanadoo.fr>
 */

#ifndef OPROF_REPORT_H
#define OPROF_REPORT_H

#include <string>

#include "ui/oprof_report.base.h"

class samples_container_t;
class OprofppView;
class HotspotView;

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
	void load_samples_files(std::string const & filename);

	/// notify all view they have changed
	void mark_all_view_changed();

	/// the oprofpp view handling
	OprofppView * oprofpp_view;

	/// the (feel to write it) hotspot view handling
	HotspotView * hotspot_view;

	/// the data container, notification function received it when the
	/// the data changed when the view is showed for the first time.
	samples_container_t * samples_container;
};

#endif // OPROF_REPORT_H
