/**
 * @file oprofpp_view.h
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie <phil_el@wanadoo.fr>
 */

#ifndef OPROFPP_VIEW_H
#define OPROFPP_VIEW_H

#include "op_view.h"

class QListView;
class samples_container_t;

class OprofppView : public OpView {
public:
	OprofppView(QListView * view);
private:
	void do_data_change(const samples_container_t * );
	void do_data_destroy();

	QListView * view;
};

#endif /* !OPROFPP_VIEW_H */
