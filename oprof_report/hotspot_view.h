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

#ifndef HOTSPOT_VIEW_H
#define HOTSPOT_VIEW_H

#include <vector>

#include <qwidget.h>

#include "op_view.h"

class QListView;
class samples_files_t;
class symbol_entry;

class HotspotView : public QWidget, public OpView {
public:
	HotspotView(QWidget * parent);

	/// reimplemented
	void paintEvent(QPaintEvent *);
	void do_data_change(const samples_files_t *);
	void do_data_destroy();
private:
	std::vector <const symbol_entry *> symbols;
	const samples_files_t * samples;
};

#endif /* !HOTSPOT_VIEW_H */
