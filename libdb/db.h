/**
 * @file db.h
 * This file contains various definitions and interface for management
 * of in-memory Btree.
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 */

#ifndef DB_H
#define DB_H

#include <assert.h>
#include <stddef.h>

/** the type of a key */
typedef unsigned int db_key_t;
/** the type of an information in the database */
typedef unsigned int db_value_t;

/* must be in [2-a sensible value] */
/* in function of MIN_PAGE two different search algo are selected at compile
 * time, a straight liner search or a dicho search, the best perf for the
 * two method are equal at approximately 6 and 64 item. I defer to later
 * the page size choice. See do_insert(). Test show than the better performance
 * depend on the distribution of key, e.g. for 1E6 item distributed through
 * 1E4 distinct key 64 give better performance but for 1E5 distinct key 6
 * give better performance */
#define DB_MIN_PAGE 6
#define DB_MAX_PAGE DB_MIN_PAGE*2

typedef unsigned int db_page_idx_t;
typedef unsigned int db_page_count_t;

#define db_nil_page	(db_page_idx_t)~0

/** an item */
typedef struct {
	db_page_idx_t child_page;	/**< right page index */
	db_value_t info;		/**< sample count in oprofile */
	db_key_t key;			/**< eip in oprofile */
} db_item_t;

/** a page of item */
typedef struct {
	unsigned int count;		/**< nr entry used in page_table */
	db_page_idx_t p0;		/**< left page index */
	db_item_t page_table[DB_MAX_PAGE]; /**< key, data and child index */
} db_page_t;

/** the minimal information which must be stored in the file to reload
 * properly the data base */
typedef struct {
	db_page_count_t size;		/**< in page nr */
	db_page_count_t current_size;	/**< nr used page */
	db_page_idx_t root_idx;		/**< the root page index */
	int padding[5];			/**< for padding and future use */
} db_descr_t;

/** a "database". this is an in memory only description.
 *
 * We allow to manage a database inside a mapped file with an "header" of
 * unknown so db_open get a parameter to specify the size of this header.
 * A typical use is:
 *
 * struct header { int etc; ... };
 * db_open(&tree, filename, DB_RW, sizeof(header));
 * so on this library have no dependency on the header type.
 */
typedef struct {
	db_page_t * page_base;		/**< base memory area of the page */
	int fd;				/**< file descriptor of the maped mem */
	void * base_memory;		/**< base memory of the maped memory */
	db_descr_t * descr;		/**< the current state of database */
	unsigned int sizeof_header;	/**< from base_memory to descr */
	unsigned int offset_page;	/**< from base_memory to page_base */
	int is_locked;			/**< is fd already locked */
} db_tree_t;

#ifdef __cplusplus
extern "C" {
#endif

/* db-manage.c */

/** how to open the DB tree file */
enum db_rw {
	DB_RDONLY = 0, /**< open for read only */
	DB_RDWR = 1 /**< open for read and/or write */
};

/**
 * db_open - open a DB tree file
 * @param tree the data base object to setup
 * @param filename the filename where go the maped memory
 * @param rw \enum DB_RW if opening for writing, else \enum DB_RDONLY
 * @param sizeof_header size of the file header if any
 *
 * The sizeof_header parameter allows the data file to have a header
 * at the start of the file which is skipped.
 * db_open() always preallocate a few number of pages.
 */
void db_open(db_tree_t * tree, char const * filename, enum db_rw rw, size_t sizeof_header);

/** Close the given DB tree */
void db_close(db_tree_t * tree);

/** issue a msync on the used size of the mmaped file */
void db_sync(db_tree_t const * tree);

/** add a page returning its index. Take care all page pointer can be
 * invalidated by this call ! */
db_page_idx_t db_add_page(db_tree_t * tree);

/** db-debug.c */
/** check than the tree is well build by making a db_check_page_pointer() then
 * checking than item are correctly sorted */
int db_check_tree(const db_tree_t * tree);
/** check than child page nr are coherent */
int db_check_page_pointer(db_tree_t const * tree);
/** display the item in tree */
void db_display_tree(db_tree_t const * tree);
/** same as above but do not travel through the tree, just display raw page */
void db_raw_display_tree(db_tree_t const * tree);

/* db-insert.c */
/** insert info at key, if key already exist the info is added to the
 * existing samples */
void db_insert(db_tree_t * tree, db_key_t key, db_value_t info);

/* db-travel.c */
/** the call back type to pass to travel() */
typedef void (*db_travel_callback)(db_key_t key, db_value_t info, void * data);
/** iterate through key in rang [first, last[ passing it to callback,
 * data is optional user data to pass to the callback */
void db_travel(db_tree_t const * tree, db_key_t first, db_key_t last,
	       db_travel_callback callback, void * data);

/** from a page index return a page pointer */
static __inline db_page_t * page_nr_to_page_ptr(db_tree_t const * tree,
						db_page_idx_t page_nr)
{
	assert(page_nr < tree->descr->current_size);
	return &tree->page_base[page_nr];
}

#ifdef __cplusplus
}
#endif

#endif /* !DB_H */
