/**
 * @file hotspot_view.h
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie <phil_el@wanadoo.fr>
 * @author John Levon <moz@compsoc.man.ac.uk>
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
