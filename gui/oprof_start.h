/**
 * @file oprof_start.h
 * The GUI start main class
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie <phil_el@wanadoo.fr>
 * @author John Levon <moz@compsoc.man.ac.uk>
 */

#ifndef OPROF_START_H
#define OPROF_START_H

#include <vector>

#include "ui/oprof_start.base.h"
#include "oprof_start_config.h"
#include "oprof_start_util.h"
#include "persistent_config.h"

#include "op_hw_config.h"
#include "op_events.h"
#include "op_events_desc.h"

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
	const op_unit_mask * unit;
	/// unit mask descriptions if applicable
	const op_unit_desc * um_desc;
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
	/// select the System.map filename
	void choose_system_map_filename();
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
	/// enabled has been changed
	void enabled_toggled(bool); 

	/// close the dialog
	void accept();

	/// WM hide event
	void closeEvent(QCloseEvent * e);
 
	/// timer event 
	void timerEvent(QTimerEvent * e);

private:
	/// find an event description by name
	const op_event_descr & locate_event(std::string const & name);

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
	void display_event(struct op_event_descr const * descrp);
 
	/// hide unit mask widgets
	void hide_masks(void);
 
	/// update the counter combo at given position
	void set_counter_combo(uint);
 
	/// load the event config file
	void load_event_config_file(uint ctr);
	/// save the event config file 
	bool save_event_config_file(uint ctr);
	/// load the extra config file
	void load_config_file();
	/// save the extra config file
	bool save_config_file();

	/// validator for event count
	QIntValidator* event_count_validator;

	/// all available events for this hardware
	std::vector<op_event_descr> v_events;

	/// the current counter in the GUI
	uint current_ctr;
	/// current event selections for each counter
	struct op_event_descr const * current_event[OP_MAX_COUNTERS];
	/// current event configs for each counter
	persistent_config_t<event_setting> event_cfgs[OP_MAX_COUNTERS];
	/// enabled status for each counter
	bool ctr_enabled[OP_MAX_COUNTERS]; 
 
	/// current config
	config_setting config;

	/// the expansion of "~" directory
	std::string user_dir;

	/// CPU type
	op_cpu cpu_type;

	/// CPU speed in MHz
	unsigned long cpu_speed;
 
	/// total number of available HW counters
	uint op_nr_counters;

	/// Total number of samples for this run
	unsigned long total_nr_interrupts;
};

#endif // OPROF_START_H
