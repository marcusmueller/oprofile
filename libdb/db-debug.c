#include <stdio.h>
#include <stdlib.h>

#include "db.h"


static void do_display_tree(const db_tree_t * tree, db_page_idx_t page_idx)
{
	size_t i;
	db_page_t * page;

	if (page_idx == db_nil_page)
		return;

	page = page_nr_to_page_ptr(tree, page_idx);

	do_display_tree(tree, page->p0);

	for (i = 0 ; i < page->count ; ++i) {
		printf("%d\t", page->page_table[i].key);
		do_display_tree(tree, page->page_table[i].child_page);
	}
}

void db_display_tree(const db_tree_t * tree)
{
	do_display_tree(tree, tree->descr->root_idx);
}

static void do_raw_display_tree(const db_tree_t * tree)
{
	size_t i;
	printf("tree root %d\n", tree->descr->root_idx);
	for (i = 0 ; i < tree->descr->current_size ; ++i) {
		db_page_t * page;
		size_t j;

		page = page_nr_to_page_ptr(tree, i);
		printf("p0: %d\n", page->p0);
		for (j = 0 ; j < page->count ; ++j) {
			printf("(%d, %d, %d)\n",
			       page->page_table[j].key,
			       page->page_table[j].info,
			       page->page_table[j].child_page);
		}
		printf("\n");
	}
}

void db_raw_display_tree(const db_tree_t * tree)
{
	do_raw_display_tree(tree);
}

static int do_check_page_pointer(const db_tree_t * tree,
				 db_page_idx_t page_idx, int * viewed_page)
{
	int ret;
	size_t i;
	const db_page_t * page;

	if (page_idx == db_nil_page)
		return 0;

	if (page_idx >= tree->descr->current_size) {
		printf("%s:%d invalid page number, max is %d page_nr is %d\n",
		       __FILE__, __LINE__, tree->descr->current_size,
		       page_idx);
		return 1;
	}

	if (viewed_page[page_idx]) {
		printf("%s:%d child page number duplicated %d\n",
		       __FILE__, __LINE__, page_idx);
		return 1;
	}

	viewed_page[page_idx] = 1;

	page = page_nr_to_page_ptr(tree, page_idx);

	ret = do_check_page_pointer(tree, page->p0, viewed_page);

	for (i = 0 ; i < page->count ; ++i) {
		ret |= do_check_page_pointer(tree,
					     page->page_table[i].child_page,
					     viewed_page);
	}

	/* this is not a bug, item at pos >= page->count are in an undefined
	 * state */
/*
	for ( ; i < MAX_PAGE ; ++i) {
		if (page->page_table[i].child_page != db_nil_page) {
			ret = 1;
		}
	}
*/

	return ret;
}

int db_check_page_pointer(const db_tree_t * tree)
{
	int ret;
	int * viewed_page;

	if (tree->descr->current_size > tree->descr->size) {
		printf("%s:%d invalid current size %d, %d\n",
		       __FILE__, __LINE__,
		       tree->descr->current_size, tree->descr->size);
	}

	viewed_page = calloc(tree->descr->current_size, sizeof(int));

	ret = do_check_page_pointer(tree, tree->descr->root_idx, viewed_page);

	free(viewed_page);

	return ret;
}

static int do_check_tree(const db_tree_t * tree, db_page_idx_t page_nr, 
			 db_key_t last)
{
	size_t i;
	const db_page_t * page;

	page = page_nr_to_page_ptr(tree, page_nr);

	if (page->p0 != db_nil_page)
		do_check_tree(tree, page->p0, last);

	for (i = 0 ; i < page->count ; ++i) {
		if (page->page_table[i].key <= last) {
			return 1;
		}
		last = page->page_table[i].key;
		if (page->page_table[i].child_page != db_nil_page)
			if (do_check_tree(tree, page->page_table[i].child_page,
				       last))
				return 1;
	}

	return 0;
}

int db_check_tree(const db_tree_t * tree)
{
	int ret = db_check_page_pointer(tree);
	if (!ret)
		ret = do_check_tree(tree, tree->descr->root_idx, 0u);

	return ret;
}
