#include <stdio.h>

#include "db.h"

static void do_travel(const db_tree_t * tree, db_page_idx_t page_idx,
		      db_key_t first, db_key_t last,
		      db_travel_callback callback, void * data)
{
	size_t pos;
	db_page_t * page;
	db_page_idx_t child_page_idx = db_nil_page;

	if (page_idx == db_nil_page)
		return;

	page = page_nr_to_page_ptr(tree, page_idx);

	/* for now I use a linear search until we choose the underlined page
	 * size (medium or little) */
	for (pos = 0 ; pos < page->count ; ++pos) {
		if (page->page_table[pos].key >= first)
			break;
	}

	if (pos == page->count || page->page_table[pos].key != first) {
		if (pos == 0) {
			child_page_idx = page->p0;
		} else {
			child_page_idx = page->page_table[pos - 1].child_page;
		}
	}

	do_travel(tree, child_page_idx, first, last, callback, data);

	for ( ; pos < page->count && page->page_table[pos].key < last; ++pos) {

		callback(page->page_table[pos].key, page->page_table[pos].info,
			 data);

		do_travel(tree, page->page_table[pos].child_page,
			  first, last, callback, data);
	}
}

void db_travel(const db_tree_t * tree, db_key_t first, db_key_t last,
	    db_travel_callback callback, void * data)
{
	do_travel(tree, tree->descr->root_idx, first, last, callback, data);
}
