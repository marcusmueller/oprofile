/**
 * @file oprofpp_view.cpp
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 */

#include <stdio.h>

#include <string>

#include <qvariant.h>
#include <qlistview.h>

#include "../pp/samples_container.h"

#include "oprofpp_view.h"

using std::string;

/**
 * implementation of oprofpp_view: an oprofpp_view is a list view of symbol.
 * Each Symbol can have sub item which provide details for for this symbol.
 *
 * symbol item are modelised through SymbolItem class and details through
 * SampleItem class object.
 */

/// the class decribing an item in view
class SymbolItem : public QListViewItem {
public:
	SymbolItem(QListView* parent, symbol_entry const * symbol_,
		   samples_container_t const & samples_container_);

	/// reimplemented
	QString text(int column) const;
	/// reimplemented
	QString key(int column, bool ascending) const;
	/// reimplemented
	void setOpen(bool open);
private:
	symbol_entry const * symbol;
	/// needed mainly for statistics purpose
	samples_container_t const & samples_container;
};

/// the class decribing a detailed item in view
class SampleItem : public QListViewItem {
public:
	SampleItem(QListViewItem * parent, sample_entry const & sample_,
		   samples_container_t const & samples_container_);

	/// reimplemented
	QString text(int column) const;
	/// reimplemented
	QString key(int column, bool ascending) const;
private:
	sample_entry const & sample;
	samples_container_t const & samples_container;
};

// the column description of an oprofpp_view
struct ColumnDescr {
	char const * column_name;
	// is by default this column showed
	bool default_column;
};

// add new column at the end only please, order is meaningfull, see
// get_text() and get_key()
static struct ColumnDescr column_descr[] = {
	{ "vma", true },
	{ "samples", true },
	{ "percent", true },
	{ "symbol name", true }
};

// only to clarify code of get_text(), get_key(), ordering must be the same
// as used for column_descr. Note than the numerotation of column header
// depend on order creation of column. So ordering remains valid even the
// user change the column order.
enum ColumnId {
	cid_vma,
	cid_samples,
	cid_percent,
	cid_symbol_name
};

size_t const nr_column_descr = sizeof(column_descr) / sizeof(column_descr[0]);

/**
 * get_text() - helper function to get the text string for a column
 */
static QString get_text(sample_entry const & sample,
			string const & name,
			samples_container_t const & samples_container,
			int column)
{
	char buffer[256];
	double ratio;
	switch (column) {
		case cid_vma:
			sprintf(buffer, "%08lx", sample.vma);
			return QString(buffer);
		case cid_samples:
			return QString().setNum(sample.counter[0]);
		case cid_percent:
			ratio = double(sample.counter[0])
					/ samples_container.samples_count(0);
			sprintf(buffer, "%2.4f",  ratio * 100);
			return QString(buffer);
		case cid_symbol_name:
			return QString(name.c_str());
	}

	return 0;
}

/**
 * get_key() - return the key string for a column. When the text string for
 * this column is suitable for sorting purpose we return the get_text() string
 */
static QString get_key(sample_entry const & sample,
		       string const & name,
		       samples_container_t const & samples_container,
		       int column)
{
	char buffer[32];
	switch (column) {
		case cid_vma:
		case cid_symbol_name:
		  return get_text(sample, name,
				  samples_container, column);
		// lexical sort does not work with floating point but the
		// percent sort is identical to the samples nr sort so use it
		case cid_percent:
		case cid_samples:
			sprintf(buffer, "%09d", sample.counter[0]);
			return QString(buffer);
	}

	return 0;
}

/**
 * SymbolItem - ctor
 */
SymbolItem::SymbolItem(QListView* parent, symbol_entry const * symbol_,
		       samples_container_t const & samples_container_)
	:
	QListViewItem(parent),
	symbol(symbol_),
	samples_container(samples_container_)
{
	if (symbol->first != symbol->last)
		setExpandable(true);
}

/**
 * text - reimplemented from QListViewItem, see get_text()
 */
QString SymbolItem::text(int column) const
{
	return get_text(symbol->sample, symbol->name,
			samples_container, column);
}

/**
 * key - reimplemented from QListViewItem, see get_key()
 */
QString SymbolItem::key(int column, bool) const
{
	return get_key(symbol->sample, symbol->name,
		       samples_container, column);
}

/**
 * setOpen - reimplemented from QListViewItem
 *
 * lazilly build samples item when a symbol item is opened
 * for the first time
 */
void SymbolItem::setOpen(bool open)
{
	if (open && !childCount()) {
		for (size_t i = symbol->first ; i != symbol->last ; ++i) {
			sample_entry const & sample =
				samples_container.get_samples(i);

			new SampleItem(this, sample, samples_container);
		}
	}

	QListViewItem::setOpen(open);
}

/**
 * SampleItem - build a sample item which appears as a sub item
 * (details for a symbol)
 */
SampleItem::SampleItem(QListViewItem * parent, sample_entry const & sample_,
		       samples_container_t const & samples_container_)
	:
	QListViewItem(parent),
	sample(sample_),
	samples_container(samples_container_)
{
}

/**
 * text - reimplemented from QListViewItem, see get_text()
 */
QString SampleItem::text(int column) const
{
	return get_text(sample, string(), samples_container, column);
}

/**
 * key - reimplemented from QListViewItem, see get_key()
 */
QString SampleItem::key(int column, bool) const
{
	return get_text(sample, string(), samples_container, column);
}

/**
 * oprofpp_view - setup the view
 */
OprofppView::OprofppView(QListView * view_)
	:
	view(view_)
{
	view->setSorting(cid_samples, false);

	for (size_t i = 0 ; i < nr_column_descr ; ++i) {
		view->addColumn(column_descr[i].column_name);
	}
}

/**
 * do_data_change - create and insert items in the view
 */
void OprofppView::do_data_change(const samples_container_t * samples)
{
	vector<symbol_entry const *> symbs;
	samples->select_symbols(symbs, 0, 0.0, false, true);

	vector<symbol_entry const *>::const_iterator it;
	for (it = symbs.begin() ; it != symbs.end() ; ++it) {
		new SymbolItem(view, *it, *samples);
	}
}


/**
 * do_data_destroy - destroy all items in list view
 */
void OprofppView::do_data_destroy()
{
	view->clear();
}

