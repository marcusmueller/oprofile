/**
 * @file oprof_start.cpp
 * The GUI start main class
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie <phil_el@wanadoo.fr>
 * @author John Levon <moz@compsoc.man.ac.uk>
 */

#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

#include <cstdio>
#include <cmath>
#include <sstream>
#include <iostream>
#include <fstream>
#include <algorithm>

#include <qlineedit.h>
#include <qlistview.h>
#include <qcombobox.h>
#include <qlistbox.h>
#include <qfiledialog.h>
#include <qbuttongroup.h>
#include <qcheckbox.h>
#include <qtabwidget.h>
#include <qmessagebox.h>
#include <qvalidator.h>
#include <qlabel.h>
#include <qpushbutton.h>

#include "oprof_start.h"
#include "op_config.h"
#include "string_manip.h"

using std::string;

op_event_descr::op_event_descr()
	:
	counter_mask(0),
	val(0),
	unit(0),
	um_desc(0),
	min_count(0)
{
}

oprof_start::oprof_start()
	:
	oprof_start_base(0, 0, false, 0),
	event_count_validator(new QIntValidator(event_count_edit)),
	current_ctr(0),
	cpu_speed(get_cpu_speed()),
	op_nr_counters(2),
	total_nr_interrupts(0)
{
	for (uint i = 0; i < OP_MAX_COUNTERS; ++i) {
		current_event[i] = 0;
		ctr_enabled[i] = 0;
	}

	std::vector<std::string> args;
	args.push_back("oprofile");

	if (do_exec_command("/sbin/modprobe", args))
		exit(EXIT_FAILURE);

	cpu_type = op_get_cpu_type();

	if (cpu_type == CPU_ATHLON)
		op_nr_counters = 4;
	else if (cpu_type == CPU_RTC) {
		op_nr_counters = 1;
		current_ctr = 0;
		enabled_toggled(1);
		enabled->hide();
		counter_combo->hide();
		unit_mask_group->hide();
	}

	int cpu_mask = 1 << cpu_type;

	// check if our cpu type match with the cpu type in config file, if we
	// mismatch just delete all the oprof_start config file else we
	// confuse later code.
	string config_dir = get_user_filename(".oprofile");
	string config_name = config_dir + "/oprof_start_config";

	std::ifstream in(config_name.c_str());

	bool delete_all_config_file = !in;

	if (in) {
		int tmp;
		in >> tmp;
		op_cpu tmp_cpu_type = static_cast<op_cpu>(tmp);

		if (tmp_cpu_type != cpu_type) {
			remove(config_name.c_str());

			delete_all_config_file = true;
		}
	}

	if (delete_all_config_file) {
		for (uint ctr = 0 ; ctr < OP_MAX_COUNTERS ; ++ctr) {
			std::ostringstream name;

			name << config_dir << "/oprof_start_event"
			     << "#" << ctr;

			remove(name.str().c_str());
		}
	}

	// we need to build the event descr stuff before loading the
	// configuration because we use locate_events to get an event descr
	// from its name.
	for (uint i = 0 ; i < op_nr_events ; ++i) {
		if (!(op_events[i].cpu_mask & cpu_mask))
			continue;

		op_event_descr descr;

		descr.counter_mask = op_events[i].counter_mask;
		descr.val = op_events[i].val;
		if (op_events[i].unit) {
			descr.unit = &op_unit_masks[op_events[i].unit];
			descr.um_desc = &op_unit_descs[op_events[i].unit];
		} else {
			descr.unit = 0;
			descr.um_desc = 0;
		}

		descr.name = op_events[i].name;
		descr.help_str = op_event_descs[i];
		descr.min_count = op_events[i].min_count;

		for (uint ctr = 0; ctr < op_nr_counters; ++ctr) {
			uint count;

			if (!(descr.counter_mask & (1 << ctr)))
				continue;

			if (cpu_type == CPU_RTC) {
				count = 1024;
			} else {
				/* setting to cpu Hz / 2000 gives a safe value for
				 * all events, and a good one for most.
				 */
				if (cpu_speed)
					count = cpu_speed * 500;
				else
					count = descr.min_count * 100;
			}

			event_cfgs[ctr][descr.name].count = count;
			event_cfgs[ctr][descr.name].umask = 0;
			if (descr.unit)
				event_cfgs[ctr][descr.name].umask = descr.unit->default_mask;
			event_cfgs[ctr][descr.name].os_ring_count = 1;
			event_cfgs[ctr][descr.name].user_ring_count = 1;
		}

		v_events.push_back(descr);
	}

	load_config_file();

	// setup the configuration page.
	kernel_filename_edit->setText(config.kernel_filename.c_str());

	kernel_range_auto_cb->setChecked(config.kernel_range_auto);
	kernel_start_edit->setText(QString().setNum(config.kernel_start, 16));
	kernel_end_edit->setText(QString().setNum(config.kernel_end, 16));

	// if config.kernel_range_auto is off we don't parse vmlinux, we rely
	// on op_start to make this job : parsing the output of nm is easy in
	// shell script and the only drawback is that we don't show the kernel
	// range in the edit range widget. Feel free to ehance this stuff ...

	buffer_size_edit->setText(QString().setNum(config.buffer_size));
	hash_table_size_edit->setText(QString().setNum(config.hash_table_size));
	note_table_size_edit->setText(QString().setNum(config.note_table_size));
	if (config.pid_filter)
		pid_filter_edit->setText(QString().setNum(config.pid_filter));
	else
		pid_filter_edit->setText("");
	if (config.pgrp_filter)
		pgrp_filter_edit->setText(QString().setNum(config.pgrp_filter));
	else
		pgrp_filter_edit->setText("");
	ignore_daemon_samples_cb->setChecked(config.ignore_daemon_samples);
	verbose->setChecked(config.verbose);
	kernel_only_cb->setChecked(config.kernel_only);
	separate_samples_cb->setChecked(config.separate_samples);

	// the unit mask check boxes
	hide_masks();

	event_count_edit->setValidator(event_count_validator);
	QIntValidator * iv;
	iv = new QIntValidator(OP_MIN_BUF_SIZE, OP_MAX_BUF_SIZE, buffer_size_edit);
	buffer_size_edit->setValidator(iv);
	iv = new QIntValidator(OP_MIN_HASH_SIZE, OP_MAX_HASH_SIZE, hash_table_size_edit);
	hash_table_size_edit->setValidator(iv);
	iv = new QIntValidator(OP_MIN_NOTE_TABLE_SIZE, OP_MAX_NOTE_TABLE_SIZE, note_table_size_edit);
	note_table_size_edit->setValidator(iv);
	iv = new QIntValidator(OP_MIN_PID, OP_MAX_PID, pid_filter_edit);
	pid_filter_edit->setValidator(iv);
	iv = new QIntValidator(OP_MIN_PGRP, OP_MAX_PGRP, pgrp_filter_edit);
	pgrp_filter_edit->setValidator(iv);

	events_list->setSorting(-1);

	for (uint ctr = 0 ; ctr < op_nr_counters ; ++ctr) {
		load_event_config_file(ctr);
		counter_combo->insertItem("");
		set_counter_combo(ctr);
	}

	// re-init event stuff
	enabled_toggled(ctr_enabled[current_ctr]);
	display_event(current_event[current_ctr]);

	counter_selected(0);

	if (cpu_type == CPU_RTC)
		events_list->setCurrentItem(events_list->firstChild());

	// daemon status timer
	startTimer(5000);
	timerEvent(0);
}

// load the configuration, if the configuration file does not exist create it.
// the parent directory of the config file is created if necessary through
// save_config_file().
void oprof_start::load_config_file()
{
	std::string name = get_user_filename(".oprofile/oprof_start_config");

	{
		std::ifstream in(name.c_str());
		// this creates the config directory if necessary
		if (!in)
			save_config_file();
	}

	std::ifstream in(name.c_str());
	if (!in) {
		QMessageBox::warning(this, 0, "Unable to open configuration "
				     "~/.oprofile/oprof_start_config");
		return;
	}

	int tmp;
	in >> tmp;
	op_cpu tmp_cpu_type = static_cast<op_cpu>(tmp);

	if (tmp_cpu_type != cpu_type) {
		/* can never happen, if cpu type mismatch the file should be
		 * already deleted */
		QMessageBox::warning(this, 0,
				     "The cpu type in your configuration "
				     "mismatch the current cpu core:\n\n"
				     "Delete manually the configuration file");

		exit(EXIT_FAILURE);
	}

	for (uint i = 0; i < op_nr_counters; ++i) {
		in >> ctr_enabled[i];
		std::string ev;
		in >> ev;
		if (ev == "none")
			current_event[i] = 0;
		else
			current_event[i] = &locate_event(ev);
	}

	in >> config;
}

// save the configuration by overwritting the configuration file if it exist or
// create it. The parent directory of the config file is created if necessary
bool oprof_start::save_config_file()
{
	if (!check_and_create_config_dir())
		return false;

	std::string name = get_user_filename(".oprofile/oprof_start_config");

	std::ofstream out(name.c_str());

	out << static_cast<int>(cpu_type) << " ";

	for (uint i = 0; i < op_nr_counters; ++i) {
		out << ctr_enabled[i];
		if (current_event[i] == 0)
			out << " none ";
		else
			out << " " << current_event[i]->name + " ";
	}
	out << std::endl;

	out << config;

	return true;
}

// this work as load_config_file()/save_config_file()
void oprof_start::load_event_config_file(uint ctr)
{
	std::ostringstream name;

	name << get_user_filename(".oprofile/oprof_start_event");
	name << "#" << ctr;

	{
		std::ifstream in(name.str().c_str());
		// this creates the config directory if necessary
		if (!in)
			save_event_config_file(ctr);
	}

	std::ifstream in(name.str().c_str());
	if (!in) {
		QMessageBox::warning(this, 0, "Unable to open configuration "
				     "~/.oprofile/oprof_start_event");
		return;
	}

	// need checking on the key validity :(
	in >> event_cfgs[ctr];
}

// this work as load_config_file()/save_config_file()
bool oprof_start::save_event_config_file(uint ctr)
{
	if (!check_and_create_config_dir())
		return false;

	std::ostringstream name;

	name << get_user_filename(".oprofile/oprof_start_event");
	name << "#" << ctr;

	std::ofstream out(name.str().c_str());
	if (!out)
		return false;

	out << event_cfgs[ctr];

	return true;
}

// user request a "normal" exit so save the config file.
void oprof_start::accept()
{
	// record the previous settings
	record_selected_event_config();

	// FIXME: check and warn about return code.
	for (uint ctr = 0 ; ctr < op_nr_counters ; ++ctr)
		save_event_config_file(ctr);

	if (!record_config())
		return;

	save_config_file();

	QDialog::accept();
}


void oprof_start::closeEvent(QCloseEvent *)
{
	accept();
}


void oprof_start::timerEvent(QTimerEvent *)
{
	static time_t last = time(0);

	daemon_status dstat;

	flush_profiler_data_btn->setEnabled(dstat.running);
	stop_profiler_btn->setEnabled(dstat.running);
	start_profiler_btn->setEnabled(!dstat.running);

	if (!dstat.running) {
		daemon_label->setText("Profiler is not running.");
		return;
	}

	std::ostringstream ss;
	ss << "Profiler running ";
	ss << dstat.runtime;

	time_t curr = time(0);
	total_nr_interrupts += dstat.nr_interrupts;

	if (curr - last)
		ss << " (" << dstat.nr_interrupts / (curr - last) << " interrupts / second, total " << total_nr_interrupts << ")";

	daemon_label->setText(ss.str().c_str());

	last = curr;
}


void oprof_start::set_counter_combo(uint ctr)
{
	std::string ctrstr("Counter ");
	char c = '0' + ctr;
	ctrstr += c;
	ctrstr += std::string(": ");
	if (current_event[ctr])
		ctrstr += current_event[ctr]->name;
	else
		ctrstr += "not used";
	counter_combo->changeItem(ctrstr.c_str(), ctr);
	counter_combo->setMinimumSize(counter_combo->sizeHint());
}


void oprof_start::counter_selected(int ctr)
{
	setUpdatesEnabled(false);
	events_list->clear();

	if (current_event[current_ctr])
		record_selected_event_config();

	current_ctr = ctr;

	display_event(current_event[current_ctr]);

	QListViewItem * theitem = 0;

	for (std::vector<op_event_descr>::reverse_iterator cit = v_events.rbegin();
		cit != v_events.rend(); ++cit) {
		if (cit->counter_mask & (1 << ctr)) {
			QListViewItem * item = new QListViewItem(events_list, cit->name.c_str());
			if (current_event[ctr] != 0 && cit->name == current_event[ctr]->name)
				theitem = item;
		}
	}

	if (theitem) {
		events_list->setCurrentItem(theitem);
		events_list->ensureItemVisible(theitem);
	}

	enabled->setChecked(ctr_enabled[ctr]);

	setUpdatesEnabled(true);
	update();
}

void oprof_start::display_event(op_event_descr const * descrp)
{
	setUpdatesEnabled(false);

	if (!descrp) {
		os_ring_count_cb->setChecked(false);
		os_ring_count_cb->setEnabled(false);
		user_ring_count_cb->setChecked(false);
		user_ring_count_cb->setEnabled(false);
		event_count_edit->setText("");
		event_count_edit->setEnabled(false);
		hide_masks();
		return;
	}
	setup_unit_masks(*descrp);
	os_ring_count_cb->setEnabled(true);
	user_ring_count_cb->setEnabled(true);
	event_count_edit->setEnabled(true);
	persistent_config_t<event_setting> const & cfg = event_cfgs[current_ctr];

	os_ring_count_cb->setChecked(cfg[descrp->name].os_ring_count);
	user_ring_count_cb->setChecked(cfg[descrp->name].user_ring_count);
	QString count_text;
	count_text.setNum(cfg[descrp->name].count);
	event_count_edit->setText(count_text);
	event_count_validator->setRange(descrp->min_count, max_perf_count());

	event_help_label->setText(descrp->help_str.c_str());

	setUpdatesEnabled(true);
	update();
}


void oprof_start::event_selected(QListViewItem * item)
{
	op_event_descr const & descr = locate_event(item->text(0).latin1());

	if (current_event[current_ctr])
		record_selected_event_config();

	display_event(&descr);
	current_event[current_ctr] = &descr;

	set_counter_combo(current_ctr);
}


void oprof_start::event_over(QListViewItem * item)
{
	op_event_descr const & descr = locate_event(item->text(0).latin1());
	event_help_label->setText(descr.help_str.c_str());
}


void oprof_start::enabled_toggled(bool en)
{
	ctr_enabled[current_ctr] = en;
	if (!en) {
		events_list->clearSelection();
		current_event[current_ctr] = 0;
		set_counter_combo(current_ctr);
	}
	events_list->setEnabled(en);

	display_event(current_event[current_ctr]);
}


/// select the kernel image filename
void oprof_start::choose_kernel_filename()
{
	std::string name = kernel_filename_edit->text().latin1();
	std::string result = do_open_file_or_dir(name, false);

	if (!result.empty())
		kernel_filename_edit->setText(result.c_str());
}

// this record the current selected event setting in the event_cfg[] stuff.
// FIXME: need validation?
void oprof_start::record_selected_event_config()
{
	op_event_descr const * curr = current_event[current_ctr];

	if (!curr)
		return;

	persistent_config_t<event_setting>& cfg = event_cfgs[current_ctr];
	std::string name(curr->name);

	cfg[name].count = event_count_edit->text().toUInt();
	cfg[name].os_ring_count = os_ring_count_cb->isChecked();
	cfg[name].user_ring_count = user_ring_count_cb->isChecked();
	cfg[name].umask = get_unit_mask(*curr);
}

// validate and save the configuration (The qt validator installed
// are not sufficient to do the validation)
bool oprof_start::record_config()
{
	config.kernel_filename = kernel_filename_edit->text().latin1();
	config.kernel_range_auto = kernel_range_auto_cb->isChecked();
	if (!config.kernel_range_auto) {
		config.kernel_start = kernel_start_edit->text().toULong(0, 16);
		config.kernel_end = kernel_end_edit->text().toULong(0, 16);
	}

	QString const t = buffer_size_edit->text();
	uint temp = t.toUInt();
	if (temp < OP_MIN_BUF_SIZE || temp > OP_MAX_BUF_SIZE) {
		std::ostringstream error;

		error << "buffer size out of range: " << temp
		      << " valid range is [" << OP_MIN_BUF_SIZE << ", "
		      << OP_MAX_BUF_SIZE << "]";

		QMessageBox::warning(this, 0, error.str().c_str());

		return false;
	}
	config.buffer_size = temp;

	temp = hash_table_size_edit->text().toUInt();
	if (temp < OP_MIN_HASH_SIZE || temp > OP_MAX_HASH_SIZE) {
		std::ostringstream error;

		error << "hash table size out of range: " << temp
		      << " valid range is [" << OP_MIN_HASH_SIZE << ", "
		      << OP_MAX_HASH_SIZE << "]";

		QMessageBox::warning(this, 0, error.str().c_str());

		return false;
	}
	config.hash_table_size = temp;

	temp = note_table_size_edit->text().toUInt();
	if (temp < OP_MIN_NOTE_TABLE_SIZE || temp > OP_MAX_NOTE_TABLE_SIZE) {
		std::ostringstream error;

		error << "note table size out of range: " << temp
		      << " valid range is [" << OP_MIN_NOTE_TABLE_SIZE << ", "
		      << OP_MAX_NOTE_TABLE_SIZE << "]";

		QMessageBox::warning(this, 0, error.str().c_str());

		return false;
	}
	config.note_table_size = temp;

	config.pid_filter = pid_filter_edit->text().toUInt();
	config.pgrp_filter = pgrp_filter_edit->text().toUInt();
	config.ignore_daemon_samples = ignore_daemon_samples_cb->isChecked();
	config.kernel_only = kernel_only_cb->isChecked();
	config.verbose = verbose->isChecked();
	config.separate_samples = separate_samples_cb->isChecked();

	return true;
}

void oprof_start::get_unit_mask_part(op_event_descr const & descr, uint num, bool selected, uint & mask)
{
	if (!selected)
		return;
	if  (num >= descr.unit->num)
		return;

	if (descr.unit->unit_type_mask == utm_bitmask)
		mask |= descr.unit->um[num];
	else
		mask = descr.unit->um[num];
}

// return the unit mask selected through the unit mask check box
uint oprof_start::get_unit_mask(op_event_descr const & descr)
{
	uint mask = 0;

	if (!descr.unit)
		return 0;

	// mandatory mask is transparent for user.
	if (descr.unit->unit_type_mask == utm_mandatory) {
		mask = descr.unit->default_mask;
		return mask;
	}

	get_unit_mask_part(descr, 0, check0->isChecked(), mask);
	get_unit_mask_part(descr, 1, check1->isChecked(), mask);
	get_unit_mask_part(descr, 2, check2->isChecked(), mask);
	get_unit_mask_part(descr, 3, check3->isChecked(), mask);
	get_unit_mask_part(descr, 4, check4->isChecked(), mask);
	get_unit_mask_part(descr, 5, check5->isChecked(), mask);
	get_unit_mask_part(descr, 6, check6->isChecked(), mask);
	return mask;
}

void oprof_start::hide_masks()
{
	check0->hide();
	check1->hide();
	check2->hide();
	check3->hide();
	check4->hide();
	check5->hide();
	check6->hide();
}

void oprof_start::setup_unit_masks(op_event_descr const & descr)
{
	op_unit_mask const * um = descr.unit;
	op_unit_desc const * um_desc = descr.um_desc;

	hide_masks();

	if (!um || um->unit_type_mask == utm_mandatory)
		return;

	persistent_config_t<event_setting> const & cfg = event_cfgs[current_ctr];

	unit_mask_group->setExclusive(um->unit_type_mask == utm_exclusive);

	for (size_t i = 0; i < um->num ; ++i) {
		QCheckBox * check = 0;
		switch (i) {
			case 0: check = check0; break;
			case 1: check = check1; break;
			case 2: check = check2; break;
			case 3: check = check3; break;
			case 4: check = check4; break;
			case 5: check = check5; break;
			case 6: check = check6; break;
		}
		check->setText(um_desc->desc[i]);
		if (um->unit_type_mask == utm_exclusive) {
			check->setChecked(cfg[descr.name].umask == um->um[i]);
		} else {
			// The last descriptor contains a mask that enable all
			// value so we must enable the last check box only if
			// all bits are on.
			if (i == um->num - 1) {
				check->setChecked(cfg[descr.name].umask == um->um[i]);
			} else {
				check->setChecked(cfg[descr.name].umask & um->um[i]);
			}
		}
		check->show();
	}
	unit_mask_group->setMinimumSize(unit_mask_group->sizeHint());
	setup_config_tab->setMinimumSize(setup_config_tab->sizeHint());
}

uint oprof_start::max_perf_count() const
{
	return cpu_type == CPU_RTC ? OP_MAX_RTC_COUNT : OP_MAX_PERF_COUNT;
}

void oprof_start::on_flush_profiler_data()
{
	if (daemon_status().running)
		do_exec_command(BINDIR "/op_dump");
	else
		QMessageBox::warning(this, 0, "The profiler is not started.");
}

// user is happy of its setting.
void oprof_start::on_start_profiler()
{
	// save the current settings
	record_selected_event_config();

	uint c;
	for (c = 0; c < op_nr_counters; ++c) {
		if (ctr_enabled[c] && current_event[c])
			break;
	}
	if (c == op_nr_counters) {
		QMessageBox::warning(this, 0, "No counters enabled.\n");
		return;
	}

	for (uint ctr = 0; ctr < op_nr_counters; ++ctr) {
		if (!current_event[ctr])
			continue;
		if (!ctr_enabled[ctr])
			continue;

		persistent_config_t<event_setting> const & cfg = event_cfgs[ctr];

		op_event_descr const * descr = current_event[ctr];

		if (!cfg[descr->name].os_ring_count &&
		    !cfg[descr->name].user_ring_count) {
			QMessageBox::warning(this, 0, "You must select to "
					 "profile at least one of user binaries/kernel");
			return;
		}

		if (cfg[descr->name].count < descr->min_count ||
		    cfg[descr->name].count > max_perf_count()) {
			std::ostringstream out;

			out << "event " << descr->name << " count of range: "
			    << cfg[descr->name].count << " must be in [ "
			    << descr->min_count << ", "
			    << max_perf_count()
			    << "]";

			QMessageBox::warning(this, 0, out.str().c_str());
			return;
		}

		if (descr->unit &&
		    descr->unit->unit_type_mask != utm_exclusive &&
		    cfg[descr->name].umask == 0) {
			std::ostringstream out;

			out << "event " << descr->name<< " invalid unit mask: "
			    << cfg[descr->name].umask << std::endl;

			QMessageBox::warning(this, 0, out.str().c_str());
			return;
		}
	}

	if (daemon_status().running) {
		int user_choice =
			QMessageBox::warning(this, 0,
					     "Profiler already started:\n\n"
					     "stop and restart it?",
					     "&Restart", "&Cancel", 0, 0, 1);

		if (user_choice == 1)
			return;

		// this flush profiler data also.
		on_stop_profiler();
	}

	// record_config validate the config
	if (!record_config())
		return;

	std::vector<std::string> args;

	if (cpu_type == CPU_RTC) {
		persistent_config_t<event_setting> const & cfg = event_cfgs[0];
		op_event_descr const * descr = current_event[0];
		args.push_back("--rtc-value=" + tostr(cfg[descr->name].count));
	} else {
		for (uint ctr = 0; ctr < op_nr_counters; ++ctr) {
			if (!current_event[ctr])
				continue;
			if (!ctr_enabled[ctr])
				continue;

			persistent_config_t<event_setting> const & cfg = event_cfgs[ctr];

			op_event_descr const * descr = current_event[ctr];

			args.push_back("--ctr" + tostr(ctr) + "-event=" + descr->name);
			args.push_back("--ctr" + tostr(ctr) + "-count=" + tostr(cfg[descr->name].count));
			args.push_back("--ctr" + tostr(ctr) + "-kernel=" + tostr(cfg[descr->name].os_ring_count));
			args.push_back("--ctr" + tostr(ctr) + "-user=" + tostr(cfg[descr->name].user_ring_count));

			if (descr->um_desc)
				args.push_back("--ctr" + tostr(ctr) + "-unit-mask=" + tostr(cfg[descr->name].umask));
		}
	}

	if (!config.kernel_range_auto) {
		std::ostringstream range;
		range << std::hex << config.kernel_start << "," << config.kernel_end;
		args.push_back("--kernel-range=" + range.str());
	}

	args.push_back("--vmlinux=" + config.kernel_filename);
	args.push_back("--kernel-only=" + tostr(config.kernel_only));
	args.push_back("--pid-filter=" + tostr(config.pid_filter));
	args.push_back("--pgrp-filter=" + tostr(config.pgrp_filter));
	args.push_back("--buffer-size=" + tostr(config.buffer_size));
	args.push_back("--hash-table-size=" + tostr(config.hash_table_size));
	args.push_back("--note-table-size=" + tostr(config.note_table_size));
	if (config.separate_samples)
		args.push_back("--separate-samples");
	if (config.verbose)
		args.push_back("--verbose");

	do_exec_command(BINDIR "/op_start", args);
	total_nr_interrupts = 0;
	timerEvent(0);
}

// flush and stop the profiler if it was started.
void oprof_start::on_stop_profiler()
{
	if (daemon_status().running)
		do_exec_command(BINDIR "/op_stop");
	else
		QMessageBox::warning(this, 0, "The profiler is already stopped.");

	timerEvent(0);
}


/// function object for matching against name
class event_name_eq : public std::unary_function<op_event_descr, bool> {
	std::string name_;
public:
	explicit event_name_eq(std::string const & s) : name_(s) {}
	bool operator()(op_event_descr & d) const {
		return d.name == name_;
	}
};

// helper to retrieve an event descr through its name.
op_event_descr const & oprof_start::locate_event(std::string const & name)
{
	return *(std::find_if(v_events.begin(), v_events.end(), event_name_eq(name)));
}
