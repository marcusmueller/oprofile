/* COPYRIGHT (C) 2001 Philippe Elie
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

#ifndef OPROF_START_H
#define OPROF_START_H

#include <vector>

#include "ui/oprof_start.base.h"
#include "oprof_start_config.h"
#include "persistent_config.h"
#include "../op_user.h"

class QIntValidator;
class QListViewItem;

struct op_event_descr {
	op_event_descr();

	uint counter_mask;		// bitmask of allowed counter
	u8 val;				// event number
	const op_unit_mask* unit;	// != 0 if unit mask allowed
	const op_unit_desc* um_descr;	// ditto
	// FIXME: char arrays ... NOOOO ! ;) 
	const char* name;		// never nil
	const char* help_str;		// ditto
	uint min_count;			// minimum counter value allowed
	QCheckBox* cb;
};

class oprof_start : public oprof_start_base
{
	Q_OBJECT

public:
	oprof_start();

protected:
	void on_choose_file_or_dir();
	void on_event_clicked();
	void on_flush_profiler_data();
	void on_start_profiler();
	void on_stop_profiler();
	void counter_selected(int);
	void event_selected(QListViewItem *); 

	void accept();

private:
	// return 0 if not found
	const op_event_descr* locate_event(const char* name);

	void do_selected_event_change(const op_event_descr*);
	void event_checked(const op_event_descr*);
	void event_unchecked(const op_event_descr*);

	// recorded in memory, not to persistent storage
	void record_selected_event_config();
	bool record_config();

	uint get_unit_mask_part(const QCheckBox* cb, uint result);
	uint get_unit_mask();
	void create_unit_mask_btn(const op_event_descr* descr);

	void load_event_config_file();
	bool save_event_config_file();

	void load_config_file();
	bool save_config_file();

	QIntValidator* validate_buffer_size;
	QIntValidator* validate_hash_table_size;
	QIntValidator* validate_event_count;
	QIntValidator* validate_pid_filter;
	QIntValidator* validate_pgrp_filter;

	std::vector<op_event_descr> v_events;

	// to avoid wasting cpu time when re-selecting the same event.
	//const op_event_descr* event_selected;
	// same: used to optimize mouse move event
	uint last_mouse_motion_cb_index;

	persistent_config_t<event_setting> event_cfg;
	config_setting config;

	// the expansion of "~" directory
	std::string user_dir;

	int cpu_type;
	uint op_nr_counters;
};

#endif // OPROF_START_H
