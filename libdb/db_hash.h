/**
 * @file db_hash.h
 * This file contains various definitions and interface for management
 * of in-memory, through mmaped file, growable hash table.
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 */

#ifndef DB_HASH_H
#define DB_HASH_H

#include <stddef.h>

/** the type of key */
typedef unsigned int db_key_t;
/** the type of an information in the database */
typedef unsigned int db_value_t;
/** the type of index (node number), list are implemented through index */
typedef unsigned int db_index_t;
/** the type store node number */
typedef db_index_t db_node_nr_t;
/** store the hash mask, hash table size are always power of two */
typedef db_index_t db_hash_mask_t;

/* there is (bucket factor * nr node) entry in hash table, this can seem
 * excessive but hash coding eip don't give a good distributions and our
 * goal is to get a O(1) amortized insert time. bucket factor must be a
 * power of two. FIXME: see big comment in db_hash_add_node, you must
 * re-enable zeroing hash table if BUCKET_FACTOR > 2 (roughly exact, you
 * want to read the comment in db_add_hash_node() if you tune this define) */
#define BUCKET_FACTOR 1

/** a node */
typedef struct {
	db_key_t key;			/**< eip */
	db_value_t value;		/**< samples count */
	db_index_t next;		/**< next entry for this bucket */
} db_node_t;

/** the minimal information which must be stored in the file to reload
 * properly the data base, following this header is the node array then
 * the hash table (when growing we avoid to copy node array) */
typedef struct {
	db_node_nr_t size;		/**< in node nr (power of two) */
	db_node_nr_t current_size;	/**< nr used node + 1, node 0 unused */
	int padding[6];			/**< for padding and future use */
} db_descr_t;

/** a "database". this is an in memory only description.
 *
 * We allow to manage a database inside a mapped file with an "header" of
 * unknown size so db_open get a parameter to specify the size of this header.
 * A typical use is:
 *
 * struct header { int etc; ... };
 * db_open(&hash, filename, DB_RW, sizeof(header));
 * so on this library have no dependency on the header type.
 *
 * the internal memory layout from base_memory is:
 *  the unknown header (sizeof_header)
 *  db_descr_t
 *  the node array: (descr->size * sizeof(db_node_t) entries
 *  the hash table: array of db_index_t indexing the node array 
 *    (descr->size * BUCKET_FACTOR) entries
 *
 */
typedef struct {
	db_node_t * node_base;		/**< base memory area of the page */
	db_index_t * hash_base;		/**< base memory of hash table */
	db_descr_t * descr;		/**< the current state of database */
	db_hash_mask_t hash_mask;	/**< == descr->size - 1 */
	unsigned int sizeof_header;	/**< from base_memory to db header */
	unsigned int offset_node;	/**< from base_memory to node array */
	void * base_memory;		/**< base memory of the maped memory */
	int fd;				/**< mmaped memory file descriptor */
} samples_db_t;

#ifdef __cplusplus
extern "C" {
#endif

/* db_manage.c */

/** how to open the DB hash file */
enum db_rw {
	DB_RDONLY = 0,	/**< open for read only */
	DB_RDWR = 1	/**< open for read and/or write */
};

/**
 * db_init - initialize a hash struct
 * @param hash the hash object to init
 */
void db_init(samples_db_t * hash);

/**
 * db_open - open a DB hash file
 * @param hash the data base object to setup
 * @param filename the filename where go the maped memory
 * @param rw \enum DB_RW if opening for writing, else \enum DB_RDONLY
 * @param sizeof_header size of the file header if any
 * @param err_msg error message is returned here when error occurs
 *
 * The sizeof_header parameter allows the data file to have a header
 * at the start of the file which is skipped.
 * db_open() always preallocate a few number of pages.
 * returns EXIT_SUCCESS on success, EXIT_FAILURE on failure
 * on failure *err_msg contains a pointer to an asprintf-alloced string
 * containing an error message.  the string should be freed using free() */
 int db_open(samples_db_t * hash, char const * filename, enum db_rw rw, size_t sizeof_header, char ** err_msg);

/** Close the given DB hash */
void db_close(samples_db_t * hash);

/** issue a msync on the used size of the mmaped file */
void db_sync(samples_db_t const * hash);

/** add a page returning its index. Take care all page pointer can be
 * invalidated by this call !
 * returns the index of the created node on success or
 * DB_NODE_NR_INVALID on failure
 * on failure *err_msg contains a pointer to an asprintf-alloced string
 * containing an error message.  the string should be freed using free() */
db_node_nr_t db_hash_add_node(samples_db_t * hash, char ** err_msg);

/** "immpossible" node number to indicate an error from db_hash_add_node() */
#define DB_NODE_NR_INVALID ((db_node_nr_t)-1)

/* db_debug.c */
/** check than the hash is well build */
int db_check_hash(const samples_db_t * hash);
/** display the item in hash table */
void db_display_hash(samples_db_t const * hash);
/** same as above, do not travel through the hash table but display raw node */
void db_raw_display_hash(samples_db_t const * hash);

/* db_stat.c */
typedef struct db_hash_stat_t db_hash_stat_t;
db_hash_stat_t * db_hash_stat(samples_db_t const * hash);
void db_hash_display_stat(db_hash_stat_t const * stats);
void db_hash_free_stat(db_hash_stat_t * stats);

/* db_insert.c */
/** insert info at key, if key already exist the info is added to the
 * existing samples
 * returns EXIT_SUCCESS on success, EXIT_FAILURE on failure
 * on failure *err_msg contains a pointer to an asprintf-alloced string
 * containing an error message.  the string should be freed using free() */
int db_insert(samples_db_t * hash, db_key_t key, db_value_t value, char ** err_msg);

/* db_travel.c */
/** the call back type to pass to travel() */
typedef void (*samples_db_travel_callback)(db_key_t key, db_value_t value, void * data);
/** iterate through key in range [first, last[ passing it to callback,
 * data is optional user data to pass to the callback */
/* caller woukd use the more efficient db_get_iterator() interface. This
 * interface is for debug purpose and is likely to be removed in future */
void samples_db_travel(samples_db_t const * hash, db_key_t first, db_key_t last,
	       samples_db_travel_callback callback, void * data);
/**
 * return a base pointer to the node array and number of node in this array
 * caller then will iterate through:
 *
 * db_node_nr_t node_nr, pos;
 * db_node_t * node = db_get_iterator(hash, &node_nr);
 *	for ( pos = 0 ; pos < node_nr ; ++pos) {
 *		if (node[pos].key) {
 *			// do something
 *		}
 *	}
 *
 *  note than caller *must* filter nil key.
 */
db_node_t * db_get_iterator(samples_db_t const * hash, db_node_nr_t * nr);

static __inline unsigned int do_hash(samples_db_t const * hash, db_key_t value)
{
	/* FIXME: better hash for eip value, needs to instrument code
	 * and do a lot of tests ... */
	/* trying to combine high order bits his a no-op: inside a binary image
	 * high order bits don't vary a lot, hash table start with 7 bits mask
	 * so this hash coding use 15 low order bits of eip, then add one hash
	 * bits at each grow. Hash table is stored in files avoiding to rebuild
	 * ing them at profiling re-start so on changing do_hash() change the
	 * file format */
	return ((value << 0) ^ (value >> 8)) & hash->hash_mask;
}

#ifdef __cplusplus
}
#endif

#endif /* !DB_HASH_H */
