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

#ifndef OP_VIEW_H
#define OP_VIEW_H

class QWidget;
class samples_files_t;

/**
 * OpView - the abstract base class for all view
 */
class OpView {
public:
	// ctor of derived class must add decoration
	OpView() : is_created(false) {}
	void create(const samples_files_t * samples) { 
		if (is_created == false) { 
			do_create(samples); 
			is_created = true;
		}
	}
	void destroy() {
		if (is_created == true) { do_destroy(); is_created = false; }
	} 
private:
	virtual void do_create(const samples_files_t *) = 0;
	virtual void do_destroy() = 0;
	// FUTURE
	// virtual void do_right_click();
	bool is_created;
};

#endif /* OP_VIEW_H */
