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

#include <qvalidator.h>
#include <qlineedit.h>

#include "oprof_start.h"

/* 
 *  Constructs a oprof_start which is a child of 'parent', with the 
 *  name 'name' and widget flags set to 'f' 
 *
 *  The dialog will by default be modeless, unless you set 'modal' to
 *  TRUE to construct a modal dialog.
 */
oprof_start::oprof_start( QWidget* parent,  const char* name, bool modal, WFlags fl )
	:
	oprof_start_base( parent, name, modal, fl ),
	validate_buffer_size(new QIntValidator(buffer_size_edit)),
	validate_hash_table_size(new QIntValidator(hash_table_size_edit)),
	validate_event_count(new QIntValidator(event_count_edit)),
	validate_pid_filter(new QIntValidator(pid_filter_edit)),
	validate_pgrp_filter(new QIntValidator(pgrp_filter_edit)),
	last_mouse_motion_cb_index((uint)-1),
	cpu_type(op_get_cpu_type()),
	op_nr_counters(2)
{
	if (cpu_type == CPU_ATHLON)
		op_nr_counters = 4;

	// validator range/value are set only when we have build the
	// description of events.
	buffer_size_edit->setValidator(validate_buffer_size);
	hash_table_size_edit->setValidator(validate_hash_table_size);
	event_count_edit->setValidator(validate_event_count);
	pid_filter_edit->setValidator(validate_pid_filter);
	pgrp_filter_edit->setValidator(validate_pgrp_filter);

	init();
}

/*  
 *  Destroys the object and frees any allocated resources
 */
oprof_start::~oprof_start()
{
    // no need to delete child widgets, Qt does it all for us
}
