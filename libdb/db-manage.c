/**
 * @file db-manage.c
 * Management of a DB tree
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie <phil_el@wanadoo.fr>
 */

#define _GNU_SOURCE

#include <stdlib.h>
#include <sys/fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>

#include "db.h"

static __inline db_descr_t * db_to_descr(db_tree_t * tree)
{
	return (db_descr_t*)(((char*)tree->base_memory) + tree->sizeof_header);
}

static __inline db_page_t * db_to_page(db_tree_t * tree)
{
	return (db_page_t *)(((char *)tree->base_memory) + tree->offset_page);
}

static void db_init_page(db_tree_t * tree, size_t from, size_t to)
{
	/* FIXME: db_nil_page is not currently a zero value so we need
	 * to initialize it explicitely: perhaps we can consider to
	 * not use the zero page and to use zero as db_nil_page value
	 * avoiding to touch memory of the mmaped file */
	for ( ; from != to ; ++from) {
		size_t count;
		db_page_t * page;

		/* we can't use page_nr_to_page_ptr here because
		 * page_nr_to_page_ptr can trigger an assertion ! */
		page = &tree->page_base[from];
		page->p0 = db_nil_page;
		for (count = 0 ; count < DB_MAX_PAGE ; ++count) {
			page->page_table[count].child_page = db_nil_page;
		}
	}
}

db_page_idx_t db_add_page(db_tree_t * tree)
{
	if (tree->descr->current_size >= tree->descr->size) {
		size_t old_size = tree->descr->size;
		size_t new_file_size;

		tree->descr->size *= 2;

		new_file_size = tree->descr->size * sizeof(db_page_t);
		new_file_size += tree->offset_page;

		if (ftruncate(tree->fd, new_file_size)) {
			fprintf(stderr, "unable to resize file to %d "
				"length, cause : %s\n",
				new_file_size, strerror(errno));
			exit(EXIT_FAILURE);
		}

		tree->base_memory = mremap(tree->base_memory,
			 (old_size * sizeof(db_page_t)) + tree->offset_page,
			 new_file_size, MREMAP_MAYMOVE);

		if (tree->base_memory == MAP_FAILED) {
			fprintf(stderr, "db_add_page() mremap failure "
				"cause: %s\n", strerror(errno));
			exit(EXIT_FAILURE);
		}

		tree->descr = db_to_descr(tree);
		tree->page_base = db_to_page(tree);

		db_init_page(tree, old_size, tree->descr->size);
	}

	return (db_page_idx_t)tree->descr->current_size++;
}

/* the default number of page, calculated to fit in 4096 bytes */
#define DEFAULT_PAGE_NR(offset_page)			\
	(4096 - offset_page) / sizeof(db_page_t) ?		\
	(4096 - offset_page) / sizeof(db_page_t) : 1

void db_open(db_tree_t * tree, char const * filename, enum db_rw rw, size_t sizeof_header)
{
	struct stat stat_buf;
	size_t nr_page;
	int flags = (rw == DB_RDWR) ? (O_CREAT | O_RDWR) : O_RDONLY;
	int mmflags = (rw == DB_RDWR) ? (PROT_READ | PROT_WRITE) : PROT_READ;

	memset(tree, '\0', sizeof(db_tree_t));

	tree->offset_page = sizeof_header + sizeof(db_descr_t);
	tree->sizeof_header = sizeof_header;

	tree->fd = open(filename, flags, 0644);
	if (tree->fd < 0) {
		fprintf(stderr, "db_open() fail to open %s cause: %s\n",
			filename, strerror(errno));
		exit(EXIT_FAILURE);
	}

	/* if the length of file is zero we have created it so we must grow
	 * it. Can we grow it lazilly or must the lazilly things handled
	 * by caller ? */
	if (fstat(tree->fd, &stat_buf)) {
		fprintf(stderr, "unable to stat %s cause %s\n",
			filename, strerror(errno));
		exit(EXIT_FAILURE);
	}

	if (stat_buf.st_size == 0) {
		size_t file_size;

		nr_page = DEFAULT_PAGE_NR(tree->offset_page);

		file_size = tree->offset_page + (nr_page * sizeof(db_page_t));
		if (ftruncate(tree->fd, file_size)) {
			fprintf(stderr, "unable to resize file %s to %d "
				"length, cause : %s\n",
				filename, file_size, strerror(errno));
			exit(EXIT_FAILURE);
		}
	} else {
		nr_page = (stat_buf.st_size - tree->offset_page) / sizeof(db_page_t);
	}

	tree->base_memory =
		mmap(0, (nr_page * sizeof(db_page_t)) + tree->offset_page,
			mmflags, MAP_SHARED, tree->fd, 0);

	if (tree->base_memory == MAP_FAILED) {
		fprintf(stderr, "db_add_page() mmap failure "
			"cause: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}


	tree->descr = db_to_descr(tree);
	tree->page_base = db_to_page(tree);

	if (stat_buf.st_size == 0) {
		tree->descr->size = nr_page;
		tree->descr->current_size = 0;
		tree->descr->root_idx = db_nil_page;

		db_init_page(tree, 0, tree->descr->size);
	} else {
		if (nr_page != tree->descr->size) {
			fprintf(stderr, "nr_page != tree->descr->size\n");
			exit(EXIT_FAILURE);
		}
	}
}

void db_close(db_tree_t * tree)
{
	if (tree->base_memory) {
		size_t size = (tree->descr->size * sizeof(db_page_t));
		size += tree->offset_page;

		munmap(tree->base_memory, size);
		tree->base_memory = 0;
	}

	if (tree->fd != -1) {
		close(tree->fd);
		tree->fd = -1;
	}
}

void db_sync(db_tree_t const * tree)
{
	size_t size;

	if (!tree->base_memory)
		return;

	size = tree->descr->current_size * sizeof(db_page_t);
	size += tree->offset_page;
	msync(tree->base_memory, size, MS_ASYNC);
}
