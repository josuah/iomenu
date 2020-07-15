#include "mem.h"

#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static struct mem_block *
mem_block(void **memp)
{
	struct mem_block *block = (void *)((char *)memp - sizeof *block);

	assert(memcmp(block->magic, MEM_BLOCK_MAGIC, 8) == 0);
	return block;
}

void *
mem_alloc(struct mem_pool *pool, size_t len)
{
	struct mem_block *block;

	block = calloc(1, sizeof *block + len);
	if (block == NULL)
		return NULL;
	memcpy(block->magic, MEM_BLOCK_MAGIC, 8);

	block->len = len;
	block->pool = pool;
	block->prev = NULL;
	block->next = pool->head;
	if (pool->head != NULL)
		pool->head->prev = block;
	pool->head = block;

	return block->buf;
}

int
mem_resize(void **memp, size_t len)
{
	struct mem_block *block = mem_block(*memp);
	int is_first = (block == block->pool->head);
	int is_same;
	void *v;

	v = realloc(block, sizeof *block + len);
	if (v == NULL)
		return -1;
	is_same = (block == v);
	block = v;

	block->len = len;

	if (is_same)
		return 0;

	if (block->prev != NULL)
		block->prev->next = v;
	if (block->next != NULL)
		block->next->prev = v;
	if (is_first)
		block->pool->head = v;
	*memp = block->buf;

	assert(memcmp(block->magic, MEM_BLOCK_MAGIC, 8) == 0);
	return 0;
}

int
mem_grow(void **memp, size_t len)
{
	assert(SIZE_MAX - len >= mem_block(*memp)->len);

	return mem_resize(memp, mem_length(*memp) + len);
}

int
mem_shrink(void **memp, size_t len)
{
	assert(mem_block(*memp)->len >= len);

	return mem_resize(memp, mem_length(*memp) - len);
}

size_t
mem_length(void *mem)
{
	return mem_block(mem)->len;
}

int
mem_append(void **memp, void const *buf, size_t len)
{
	size_t old_len = mem_length(*memp);
	struct mem_block *block;

	if (mem_grow(memp, len) < 0)
		return -1;
	block = mem_block(*memp);
	memcpy((char *)block->buf + old_len, buf, len);

	assert(memcmp(block->magic, MEM_BLOCK_MAGIC, 8) == 0);
	return 0;
}

int
mem_read(void **memp, struct mem_pool *pool)
{
	struct mem_block *block;
	ssize_t sz = 0;
	void *mem;

	mem = mem_alloc(pool, 0);
	if (mem == NULL)
		return -1;

	for (ssize_t r = 1; r > 0; sz += r) {
		if (mem_resize(&mem, sz + 2048) < 0)
			return -1;

		r = read(0, (char *)mem + sz, 2048);
		if (r < 0)
			return -1;
	}
	block = mem_block(mem);
	block->len = sz;

	*memp = mem;
	assert(memcmp(block->magic, MEM_BLOCK_MAGIC, 8) == 0);
	return 0;
}

void
mem_delete(void *mem)
{
	struct mem_block *block = mem_block(mem);;

	if (block == block->pool->head)
		block->pool->head = block->next;
	if (block->next != NULL)
		block->next->prev = block->prev;
	if (block->prev != NULL)
		block->prev->next = block->next;
	memset(block, 0, sizeof *block);
	free(block);
}

void
mem_free(struct mem_pool *pool)
{
	struct mem_block *block, *next;

	for (block = pool->head; block != NULL; block = next) {
		next = block->next;
		memset(block, 0, sizeof *block);
		free(block);
	}
}
