/**
 * @file op_view.h
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie <phil_el@wanadoo.fr>
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
	OpView() : notification_sended(false) {}
	void data_change(const samples_files_t * samples) { 
		if (notification_sended == false) { 
			do_data_change(samples); 
			notification_sended = true;
		}
	}
	void data_destroy() { do_data_destroy(); notification_sended = false; }
private:
	virtual void do_data_change(const samples_files_t *) = 0;
	virtual void do_data_destroy() = 0;
	// FUTURE
	// virtual void do_right_click();
	bool notification_sended;
};

#endif /* OP_VIEW_H */
