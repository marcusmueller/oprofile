/**
 * @file db-insert.c
 * Inserting a key-value pair into a DB tree
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie <phil_el@wanadoo.fr>
 */
 
#include <sys/file.h>
#include <assert.h>

#include "db.h"

static void lock_db(db_tree_t * tree)
{
	if (tree->is_locked == 0) {
		flock(tree->fd, LOCK_EX);
		tree->is_locked = 1;
	}
}

static void unlock_db(db_tree_t * tree)
{
	if (tree->is_locked) {
		flock(tree->fd, LOCK_UN);
		tree->is_locked = 0;
	}
}

static void copy_item(db_item_t * dest, db_item_t const * src,
		      size_t nr_item)
{
	size_t i;
	for (i = 0 ; i < nr_item ; ++i) {
		dest[i] = src[i];
	}
}

static void copy_item_backward(db_item_t * dest, db_item_t * src,
			       size_t nr_item)
{
	size_t i;
	for (i = nr_item ; i-- > 0 ; ) {
		dest[i] = src[i];
	}
}

static void split_page(db_tree_t * tree,  db_page_idx_t page_idx,
		       db_page_idx_t new_page_idx,
		       db_item_t * value, db_item_t * excess_elt,
		       size_t pos)
{
	db_page_t * page, * new_page;

	new_page = page_nr_to_page_ptr(tree, new_page_idx);
	page = page_nr_to_page_ptr(tree, page_idx);

	/* split page in two piece of equal size */

	if (pos < DB_MIN_PAGE) {
		/* save the pivot. */
		*excess_elt = page->page_table[DB_MIN_PAGE - 1];

		/* split-up the page. */
		copy_item(&new_page->page_table[0],
			  &page->page_table[DB_MIN_PAGE],
			  DB_MIN_PAGE);

		assert((DB_MIN_PAGE - (pos + 1)) < DB_MAX_PAGE);

		/* shift the old page to make the hole. */
		/* pos < DB_MIN_PAGE so on copy size is >= 0 */
		copy_item_backward(&page->page_table[pos + 1],
				   &page->page_table[pos],
				   DB_MIN_PAGE - (pos +  1));

		/* insert the item. */
		page->page_table[pos] = *value;
	} else if (pos > DB_MIN_PAGE) {
		/* save the pivot. */
		*excess_elt = page->page_table[DB_MIN_PAGE];

		assert((pos - (DB_MIN_PAGE+1)) < DB_MAX_PAGE);

		/* split-up the page. */
		copy_item(&new_page->page_table[0],
			  &page->page_table[DB_MIN_PAGE + 1],
			  pos - (DB_MIN_PAGE+1));

		/* insert the elt. */
		new_page->page_table[pos - (DB_MIN_PAGE + 1)] = *value;

		assert(((int)(DB_MAX_PAGE - pos)) >= 0);

		copy_item(&new_page->page_table[pos - DB_MIN_PAGE],
			  &page->page_table[pos],
			  DB_MAX_PAGE - pos);
	} else { /* pos  == DB_MIN_PAGE */
		/* the pivot is the item to insert */
		*excess_elt = *value;

		/* split-up the page */
		copy_item(&new_page->page_table[0],
			  &page->page_table[DB_MIN_PAGE],
			  DB_MIN_PAGE);
	}

	/* can setup now the page */
	page->count = new_page->count = DB_MIN_PAGE;
	new_page->p0 = excess_elt->child_page;
	excess_elt->child_page = new_page_idx;
}

static int do_reorg(db_tree_t * tree, db_page_idx_t page_idx, size_t pos,
		    db_item_t * excess_elt, db_item_t * value)
{
	int need_reorg;
	db_page_t * page;

	page = page_nr_to_page_ptr(tree, page_idx);

	assert(page->count <= DB_MAX_PAGE);
	
	/* the insertion pos can be at the end of the page so <= */
	assert(pos <= DB_MAX_PAGE);

	if (page->count < DB_MAX_PAGE) {
		/* insert at pos, shift to make a hole. */
		assert((page->count - pos < DB_MAX_PAGE));

		copy_item_backward(&page->page_table[pos + 1],
				   &page->page_table[pos],
				   page->count - pos);

		page->page_table[pos] = *value;

		page->count++;

		need_reorg = 0;
	} else {
		db_page_idx_t new_page_idx = db_add_page(tree);

		/* we can not pass page, the page pointer can be invalidated
		 * by db_add_page so pass page_idx here, call will re-get the
		 * page pointer */
		split_page(tree, page_idx, new_page_idx, value,
			   excess_elt, pos);

		need_reorg = 1;
	}

	return need_reorg;
}

#if DB_MIN_PAGE > 6
static int do_insert(db_tree_t * tree, db_page_idx_t page_idx,
		     db_item_t * excess_elt, db_item_t * value)
{
	int need_reorg;
	int left, right, pos;
	db_page_idx_t child_page_idx;
	db_page_t * page;

	page = page_nr_to_page_ptr(tree, page_idx);

	assert(page->count != 0);

	left = 0;
	right = page->count - 1;

	/* a little what experimental, compiler with cond move insn
	 * can generate code w/o branching inside the loop */
	/* tests show than 60 to 70 % of cpu time comes from this loop. */
	do {
		pos = (left + right) >> 1;
		if (page->page_table[pos].key >= value->key)
			right = pos - 1;
		if (page->page_table[pos].key <= value->key)
			left = pos + 1;
	} while (left <= right);

	/* if left - right == 2 ==> found at pos.
	 * else if right == -1 that's a leftmost elt
	 * left is insertion place */

	if (left - right == 2) {
		/* found: even if the write is non-atomic we do not need
		 * to lock() because we work only in the case of one writer,
		 * multiple reader. */
		if (page->page_table[pos].info + value->info >=
		    page->page_table[pos].info)
			page->page_table[pos].info += value->info;
		else
			/* FIXME: post profile must handle that */
			page->page_table[pos].info += (db_info_t)-1;
		return 0;
	}

	if (right == -1) {
		child_page_idx = page->p0;
	} else {
		child_page_idx = page->page_table[right].child_page;
	}

	need_reorg = 0;

	if (child_page_idx != db_nil_page) {
		need_reorg = do_insert(tree, child_page_idx,
				       excess_elt, value);
		*value = *excess_elt;
	} else {
		need_reorg = 1;
	}

	if (need_reorg) {
		lock_db(tree);
		need_reorg = do_reorg(tree, page_idx, left, excess_elt, value);
	}

	return need_reorg;
}
#else
static int do_insert(db_tree_t * tree, db_page_idx_t page_idx,
		     db_item_t * excess_elt, db_item_t * value)
{
	int need_reorg;
	size_t pos;
	db_page_idx_t child_page_idx;
	db_page_t * page;

	page = page_nr_to_page_ptr(tree, page_idx);

	assert(page->count != 0);

	for (pos = 0 ; pos < page->count ; ++pos) {
		if (page->page_table[pos].key >= value->key)
			break;
	}

	if (pos != page->count && page->page_table[pos].key == value->key) {
		/* found: even if the write is non-atomic we do not need
		 * to lock() because we work only in the case of one writer,
		 * multiple reader. */
		page->page_table[pos].info += value->info;
		return 0;
	}

	if (pos == 0) {
		child_page_idx = page->p0;
	} else {
		child_page_idx = page->page_table[pos-1].child_page;
	}

	need_reorg = 0;

	if (child_page_idx != db_nil_page) {
		need_reorg = do_insert(tree, child_page_idx,
				       excess_elt, value);
		*value = *excess_elt;
	} else {
		need_reorg = 1;
	}

	if (need_reorg) {
		lock_db(tree);
		need_reorg = do_reorg(tree, page_idx, pos, excess_elt, value);
	}

	return need_reorg;
}
#endif

void db_insert(db_tree_t * tree, db_key_t key, db_value_t info)
{
	db_item_t excess_elt;
	db_item_t value;
	int need_reorg;

	value.key = key;
	value.info = info;
	value.child_page = db_nil_page;

	if (tree->descr->root_idx == db_nil_page) {
		/* create the root. */
		db_page_t * page;

		/* we don't need to lock_db() here because we init
		 * the root index after all proper initializations */

		db_page_idx_t root_idx = db_add_page(tree);

		page = page_nr_to_page_ptr(tree, root_idx);

		page->page_table[0] = value;
		page->count = 1;

		tree->descr->root_idx = root_idx;
		return;
	}

	need_reorg = do_insert(tree, tree->descr->root_idx, &excess_elt,
			       &value);
	if (need_reorg) {
		/* increase the level of tree. */
		db_page_t * new_page;
		db_page_idx_t old_root;

		old_root = tree->descr->root_idx;
		tree->descr->root_idx = db_add_page(tree);

		/* page pointer can be invalidated by db_add_page, reload it */
		new_page = page_nr_to_page_ptr(tree, tree->descr->root_idx);

		new_page->page_table[0] = excess_elt;
		new_page->count = 1;
		new_page->p0 = old_root;
	}

	unlock_db(tree);
}

