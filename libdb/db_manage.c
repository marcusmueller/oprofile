/**
 * @file db_manage.c
 * Management of a DB hash
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 */

#define _GNU_SOURCE

#include <stdlib.h>
#include <sys/fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>

#include "odb_hash.h"

 
static inline odb_descr_t * odb_to_descr(samples_odb_t * hash)
{
	return (odb_descr_t *)(((char*)hash->base_memory) + hash->sizeof_header);
}

 
static inline odb_node_t * odb_to_node_base(samples_odb_t * hash)
{
  return (odb_node_t *)(((char *)hash->base_memory) + hash->offset_node);
}

 
static inline odb_index_t * odb_to_hash_base(samples_odb_t * hash)
{
	return (odb_index_t *)(((char *)hash->base_memory) + 
				hash->offset_node +
				(hash->descr->size * sizeof(odb_node_t)));
}

 
/**
 * return the number of bytes used by hash table, node table and header.
 */
static unsigned int tables_size(samples_odb_t const * hash, odb_node_nr_t node_nr)
{
	size_t size;

	size = node_nr * (sizeof(odb_index_t) * BUCKET_FACTOR);
	size += node_nr * sizeof(odb_node_t);
	size += hash->offset_node;

	return size;
}


void odb_set_error(samples_odb_t * hash, char const * err_msg)
{
	if (hash->err_msg) {
		fprintf(stderr, "FATAL: odb_set_error() attempt to set error "
			"whilst an error already occured\n first error is: %s",
			hash->err_msg);
		exit(EXIT_FAILURE);
	}

	hash->err_msg = err_msg;
}


odb_index_t odb_hash_add_node(samples_odb_t * hash)
{
	if (hash->descr->current_size >= hash->descr->size) {
		unsigned int old_file_size;
		unsigned int new_file_size;
		unsigned int pos;
		char * err_msg;

		old_file_size = tables_size(hash, hash->descr->size);

		hash->descr->size *= 2;

		new_file_size = tables_size(hash, hash->descr->size);

		if (ftruncate(hash->fd, new_file_size)) {
			asprintf(&err_msg, "unable to resize file to %d "
				"length, cause : %s\n",
				new_file_size, strerror(errno));
			odb_set_error(hash, err_msg);
			return ODB_NODE_NR_INVALID;
		}

		hash->base_memory = mremap(hash->base_memory,
				old_file_size, new_file_size, MREMAP_MAYMOVE);

		if (hash->base_memory == MAP_FAILED) {
			asprintf(&err_msg, "odb_hash_add_page() mremap"
				" failure cause: %s\n", strerror(errno));
			odb_set_error(hash, err_msg);
			return ODB_NODE_NR_INVALID;
		}

		hash->descr = odb_to_descr(hash);
		hash->node_base = odb_to_node_base(hash);
		hash->hash_base = odb_to_hash_base(hash);
		hash->hash_mask = (hash->descr->size * BUCKET_FACTOR) - 1;

		/* rebuild the hash table, node zero is never used. This works
		 * because layout of file is node table then hash table,
		 * sizeof(node) > sizeof(bucket) and when we grow table we
		 * double size ==> old hash table and new hash table can't
		 * overlap so on the new hash table is entirely in the new
		 * memory area (the grown part) and we know the new hash
		 * hash table is zeroed. That's why we don't need to zero init
		 * the new table */
		/* OK: the above is not exact
		 * if BUCKET_FACTOR < sizeof(bd_node_t) / sizeof(bd_node_nr_t)
		 * all things are fine and we don't need to init the hash
		 * table because in this case the new hash table is completely
		 * inside the new growed part. Avoiding to touch this memory is
		 * useful.
		 */
#if 0
		for (pos = 0 ; pos < hash->descr->size*BUCKET_FACTOR ; ++pos) {
			hash->hash_base[pos] = 0;
		}
#endif

		for (pos = 1; pos < hash->descr->current_size; ++pos) {
			odb_node_t * node = &hash->node_base[pos];
			size_t index = odb_do_hash(hash, node->key);
			node->next = hash->hash_base[index];
			hash->hash_base[index] = pos;
		}
	}

	return (odb_index_t)hash->descr->current_size++;
}

void odb_init(samples_odb_t * hash)
{
	memset(hash, '\0', sizeof(samples_odb_t));
	hash->fd = -1;
	hash->err_msg = NULL;
}

/* the default number of page, calculated to fit in 4096 bytes */
#define DEFAULT_NODE_NR(offset_node)	128

int odb_open(samples_odb_t * hash, char const * filename, enum odb_rw rw,
	size_t sizeof_header)
{
	struct stat stat_buf;
	odb_node_nr_t nr_node;
	char * err_msg;

	int flags = (rw == ODB_RDWR) ? (O_CREAT | O_RDWR) : O_RDONLY;
	int mmflags = (rw == ODB_RDWR) ? (PROT_READ | PROT_WRITE) : PROT_READ;

	memset(hash, '\0', sizeof(samples_odb_t));

	hash->offset_node = sizeof_header + sizeof(odb_descr_t);
	hash->sizeof_header = sizeof_header;

	hash->fd = open(filename, flags, 0644);
	if (hash->fd < 0) {
		asprintf(&err_msg, "odb_open(): fail to open %s cause: %s\n",
			filename, strerror(errno));
		odb_set_error(hash, err_msg);
		return EXIT_FAILURE;
	}

	if (fstat(hash->fd, &stat_buf)) {
		asprintf(&err_msg, "odb_open(): unable to stat %s cause %s\n",
			filename, strerror(errno));
		odb_set_error(hash, err_msg);
		return EXIT_FAILURE;
	}

	if (stat_buf.st_size == 0) {
		size_t file_size;

		if (rw == ODB_RDONLY) {
			asprintf(&err_msg, "odb_open() %s sample file empty\n",
				 filename);
			odb_set_error(hash, err_msg);
			return EXIT_FAILURE;
		}

		nr_node = DEFAULT_NODE_NR(hash->offset_node);

		file_size = tables_size(hash, nr_node);
		if (ftruncate(hash->fd, file_size)) {
			asprintf(&err_msg, "odb_open() unable to resize file "
				"%s to %ld length, cause : %s\n",
				filename, (unsigned long)file_size,
				strerror(errno));
			odb_set_error(hash, err_msg);
			return EXIT_FAILURE;
		}
	} else {
		/* Calculate nr node allowing a sanity check later */
		nr_node = (stat_buf.st_size - hash->offset_node) /
			((sizeof(odb_index_t) * BUCKET_FACTOR) + sizeof(odb_node_t));
	}

	hash->base_memory = mmap(0, tables_size(hash, nr_node), mmflags,
				MAP_SHARED, hash->fd, 0);

	if (hash->base_memory == MAP_FAILED) {
		asprintf(&err_msg, "odb_open() mmap failure cause: %s\n",
			strerror(errno));
		odb_set_error(hash, err_msg);
		return EXIT_FAILURE;
	}

	hash->descr = odb_to_descr(hash);

	if (stat_buf.st_size == 0) {
		hash->descr->size = nr_node;
		/* page zero is not used */
		hash->descr->current_size = 1;
	} else {
		/* file already exist, sanity check nr node */
		if (nr_node != hash->descr->size) {
			asprintf(&err_msg, "odb_open(): nr_node != "
				"hash->descr->size\n");
			odb_set_error(hash, err_msg);
			return EXIT_FAILURE;
		}
	}

	hash->hash_base = odb_to_hash_base(hash);
	hash->node_base = odb_to_node_base(hash);
	hash->hash_mask = (hash->descr->size * BUCKET_FACTOR) - 1;

	return EXIT_SUCCESS;
}


void odb_close(samples_odb_t * hash)
{
	odb_clear_error(hash);

	if (hash->base_memory) {
		size_t size = tables_size(hash, hash->descr->size);

		munmap(hash->base_memory, size);
		hash->base_memory = 0;
	}

	if (hash->fd != -1) {
		close(hash->fd);
		hash->fd = -1;
	}
}


void odb_clear_error(samples_odb_t * hash)
{
	if (hash->err_msg) {
		free((char *)hash->err_msg);
		hash->err_msg = NULL;
	}
}


void odb_sync(samples_odb_t const * hash)
{
	size_t size;

	if (!hash->base_memory)
		return;

	size = tables_size(hash, hash->descr->size);
	msync(hash->base_memory, size, MS_ASYNC);
}
