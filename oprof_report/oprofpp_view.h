/**
 * @file oprofpp_view.h
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
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
	void do_data_change(samples_container_t const * );
	void do_data_destroy();

	QListView * view;
};

#endif /* !OPROFPP_VIEW_H */
