/**
 * @file oprof_start.h
 * The GUI start main class
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 * @author John Levon
 */

#ifndef OPROF_START_H
#define OPROF_START_H

#include <vector>
#include <map>

#include "ui/oprof_start.base.h"
#include "oprof_start_config.h"
#include "oprof_start_util.h"

#include "op_hw_config.h"
#include "op_events.h"

class QIntValidator;
class QListViewItem;
class QTimerEvent;

/// a struct describing a particular event type
struct op_event_descr {
	op_event_descr();

	/// bit mask of allowed counters
	uint counter_mask;
	/// hardware event number
	u8 val;
	/// unit mask values if applicable
	op_unit_mask const * unit;
	/// name of event
	std::string name;
	/// description of event
	std::string help_str;
	/// minimum counter value
	uint min_count;
};

class oprof_start : public oprof_start_base
{
	Q_OBJECT

public:
	oprof_start();

protected slots:
	/// select the kernel image filename
	void choose_kernel_filename();
	/// flush profiler
	void on_flush_profiler_data();
	/// start profiler
	void on_start_profiler();
	/// stop profiler
	void on_stop_profiler();
	/// the counter combo has been activated
	void counter_selected(int);
	/// an event has been selected
	void event_selected(QListViewItem *);
	/// the mouse is over an event
	void event_over(QListViewItem *);

	/// close the dialog
	void accept();

	/// WM hide event
	void closeEvent(QCloseEvent * e);

	/// timer event
	void timerEvent(QTimerEvent * e);

private:
	/// fill the event details and gui setup
	void fill_events();

	/// find an event description by name
	op_event_descr const & locate_event(std::string const & name) const;

	/// update config on user change
	void record_selected_event_config();
	/// update config and validate
	bool record_config();

	/// calculate unit mask for given event and unit mask part
	void get_unit_mask_part(op_event_descr const & descr, uint num, bool selected, uint & mask);
	/// calculate unit mask for given event
	uint get_unit_mask(op_event_descr const & descr);
	/// set the unit mask widgets for given event
	void setup_unit_masks(op_event_descr const & descr);

	/// return the maximum perf counter value for the current cpu type
	uint max_perf_count() const;

	/// show an event's settings
	void display_event(op_event_descr const * descrp);

	/// hide unit mask widgets
	void hide_masks(void);

	/// update the counter combo at given position
	void set_counter_combo(uint);

	/// read the events set in daemonrc
	void read_set_events();
	/// load the extra config file
	void load_config_file();
	/// save the config
	bool save_config();

	/// validator for event count
	QIntValidator* event_count_validator;

	/// all available events for this hardware
	std::vector<op_event_descr> v_events;

	/// the current event in the GUI
	uint current_event;
	/// current event selections
	std::vector<op_event_descr const *> current_events;

	/// current event configs for each counter
	typedef std::map<std::string, event_setting> event_setting_map;
	event_setting_map event_cfgs[OP_MAX_COUNTERS];

	/// current config
	config_setting config;

	/// the expansion of "~" directory
	std::string user_dir;

	/// CPU type
	op_cpu cpu_type;

	/// CPU speed in MHz
	double cpu_speed;

	/// total number of available HW counters
	uint op_nr_counters;

	/// Total number of samples for this run
	unsigned long total_nr_interrupts;
};

#endif // OPROF_START_H
