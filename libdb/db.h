/**
 * \file db.h
 * Copyright 2002 OProfile authors
 * Read the file COPYING
 * this file contains various definitions and iterface for management
 * of in-memory Btree.
 *
 * \author Philippe Elie <phil.el@wanadoo.fr>
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

#define db_nil_page	(db_page_idx_t)~0

/** an item */
typedef struct {
	db_page_idx_t child_page;	/*< right page index */
	db_value_t info;		/*< sample count in oprofile */
	db_key_t key;			/*< eip in oprofile */
} db_item_t;

/** a page of item */
typedef struct {
	size_t  count;			/*< nr entry used in page_table */
	db_page_idx_t p0;		/*< left page index */
	db_item_t page_table[DB_MAX_PAGE]; /*< key, data and child index */
} db_page_t;

/** the minimal information which must be stored in the file to reload
 * properly the data base */
typedef struct {
	size_t size;			/*< in page nr */
	size_t current_size;		/*< nr used page */
	db_page_idx_t root_idx;		/*< the root page index */
	int padding[5];			/*< for padding and future use */
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
	db_page_t * page_base;		/*< base memory area of the page */
	int fd;				/*< file descriptor of the maped mem */
	void * base_memory;		/*< base memory of the maped memory */
	db_descr_t * descr;		/*< the current state of database */
	size_t sizeof_header;		/*< from base_memory to descr */
	size_t offset_page;		/*< from base_memory to page_base */
	size_t is_locked;		/*< is fd already locked */
} db_tree_t;

#ifdef __cplusplus
extern "C" {
#endif

/* db-manage.c */

enum db_rw {
	DB_RDONLY = 0,
	DB_RDWR = 1
};
 
/** 
 * \param tree the data base object to setup 
 * \param root_idx_ptr an external pointer to put the root index, can be null
 * \param filename the filename where go the maped memory
 * \param write %DB_RW if opening for writing, else %DB_RDONLY
 * \param offset_page offset between the mapped memory and the data base page
 * area.
 *
 * parameter root_idx_ptr and offset allow to use a data base imbeded in
 * a file containing an header such as opd_header. db_open always preallocate
 * a few number of page
 */
void db_open(db_tree_t * tree, const char * filename, enum db_rw rw, size_t sizeof_header);

/**
 * \param tree the data base to close
 */
void db_close(db_tree_t * tree);

/** issue a msync on the used size of the mmaped file */
void db_sync(db_tree_t * tree);

/** add a page returning its index. Take care all page pointer can be
 * invalidated by this call ! */
db_page_idx_t db_add_page(db_tree_t * tree);

/** db-debug.c */
/* check than the tree is well build by making a db_check_page_pointer() then
 * checking than item are correctly sorted */
int db_check_tree(const db_tree_t * tree);
/* check than child page nr are coherent */
int db_check_page_pointer(const db_tree_t * tree);
/* display the item in tree */
void db_display_tree(const db_tree_t * tree);
/* same as above but do not travel through the tree, just display raw page */
void db_raw_display_tree(const db_tree_t * tree);

/* db-insert.c */
/** insert info at key, if key already exist the info is added to the
 * existing samples */
void db_insert(db_tree_t * tree, db_key_t key, db_value_t info);

/* db-travel.c */
/** the call back type to pass to travel() */
typedef void (*db_travel_callback)(db_key_t key, db_value_t info, void * data);
/* iterate through key in rang [first, last[ passing it to callback,
 * data is an optionnal user data to pass to the callback */
void db_travel(const db_tree_t * tree, db_key_t first, db_key_t last,
	       db_travel_callback callback, void * data);

/** from a page index return a page pointer */
static __inline db_page_t * page_nr_to_page_ptr(const db_tree_t * tree,
						db_page_idx_t page_nr)
{
	assert(page_nr < tree->descr->current_size);
	return &tree->page_base[page_nr];
}

#ifdef __cplusplus
}
#endif

#endif /* !DB_H */
