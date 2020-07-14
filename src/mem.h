#ifndef MEM_H
#define MEM_H

/*
 * Lightweight wrapper over malloc, that permit to define a memory pool of
 * multiple buffers, and free them all at once.
 *
 *        *──────────┐   
 *        │ mem_pool │   
 *        ├──────────┤   
 *        │*head     │   
 *        └┬─────────┘   
 *         v
 * NULL<   *───────────┐<  >*───────────┐<  >*───────────┐<  >*───────────┐   >NULL
 *      \  │ mem_block │ \/ │ mem_block │ \/ │ mem_block │ \/ │ mem_block │  /
 *       \ ├───────────┤ /\ ├───────────┤ /\ ├───────────┤ /\ ├───────────┤ /
 *        `┤*prev *next├'  `┤*prev *next├'  `┤*prev *next├'  `┤*prev *next├'
 *         │len        │    │len        │    │len        │    │len        │
 *         ├─┴─magic───┤    ├─┴─magic───┤    ├─┴─magic───┤    ├─┴─magic───┤
 *         │///////////│    │///////////│    │///////////│    │///////////│
 *         │///////////│    │///////////│    │///////////│    │///////////│
 *         │///////////│    │///////////│    │///////////│    └───────────┘
 *         └───────────┘    │///////////│    │///////////│
 *                          │///////////│    └───────────┘
 *                          └───────────┘
 *
 * This permits the type checker to still work on all operations while
 * providing generic memory management functions for all types of data
 * structures and keep track of each object's length.
 */

#include <stddef.h>

#define MEM_BLOCK_MAGIC "\xcc\x68\x23\xd7\x9b\x7d\x39\xb9"

struct mem_pool {
	struct mem_block *head;
};

struct mem_block {
	struct mem_pool *pool;
	struct mem_block *prev, *next;
	size_t len;
	char magic[8]; /* at the end to detect buffer underflow */
	char buf[];
};

/** src/mem.c **/
void * mem_alloc(struct mem_pool *pool, size_t len);
int mem_resize(void **pp, size_t len);
int mem_grow(void **pp, size_t len);
int mem_shrink(void **pp, size_t len);
size_t mem_length(void *v);
int mem_append(void **pp, char const *buf, size_t len);
void mem_delete(void *v);
void mem_free(struct mem_pool *pool);

#endif
