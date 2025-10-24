// mem.c

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mem.h"
#include "starch.h"
#include "util.h"

enum {
	MEM_PAGE_SIZE = 0x1000,
	MEM_PAGE_MASK = (MEM_PAGE_SIZE - 1),
};

//
// Memory node
//
struct mem_node {
	struct mem_node *prev, *next;
	uint64_t addr; // Start address
	uint8_t depth; // Max following generations
	uint8_t data[MEM_PAGE_SIZE]; // Page data
};

static void mem_node_init(struct mem_node *node, uint64_t addr)
{
	memset(node, 0, sizeof(struct mem_node));
	node->addr = addr;
}

static void mem_node_destroy(struct mem_node *node)
{
	if (node->prev) {
		mem_node_destroy(node->prev);
		node->prev = NULL;
	}
	if (node->next) {
		mem_node_destroy(node->next);
		node->next = NULL;
	}
	free(node);
}

// Updates the depth of the given node based on the depths of its children
static void mem_node_update_depth(struct mem_node *node)
{
	if (node->prev) {
		uint8_t prevdepth = node->prev->depth;
		if (node->next) {
			uint8_t nextdepth = node->next->depth;
			node->depth = prevdepth > nextdepth ? prevdepth + 1 : nextdepth + 1;
		}
		else {
			node->depth = prevdepth + 1;
		}
	}
	else {
		if (node->next) {
			node->depth = node->next->depth + 1;
		}
		else {
			node->depth = 0;
		}
	}
}

// Performs a binary tree left rotation. *node must have a non-NULL next member.
static struct mem_node *mem_node_left_rotate(struct mem_node *node)
{
	struct mem_node *newroot = node->next;
	struct mem_node *mid = newroot->prev;
	newroot->prev = node;
	node->next = mid;
	mem_node_update_depth(node);
	mem_node_update_depth(newroot);
	return newroot;
}

// Performs a binary tree right rotation. *node must have a non-NULL prev member.
static struct mem_node *mem_node_right_rotate(struct mem_node *node)
{
	struct mem_node *newroot = node->prev;
	struct mem_node *mid = newroot->next;
	newroot->next = node;
	node->prev = mid;
	mem_node_update_depth(node);
	mem_node_update_depth(newroot);
	return newroot;
}

// Rebalance the tree beginning at the given node, returning the new root
static struct mem_node *mem_node_rebalance(struct mem_node *node)
{
	struct mem_node *newroot;
	if ((!node->prev && node->depth > 1) ||
		(node->prev && node->depth > node->prev->depth + 2)) {
		// node->next has too much depth
		// node->next->prev is going to end up at the same depth, while
		// node->next->next is going to move closer to root
		if (!node->next->next || node->next->next->depth < node->next->depth - 1) {
			// Greater depth comes from node->next->prev. Double-rotation is needed.
			node->next = mem_node_right_rotate(node->next);
		}
		// Left-rotate
		newroot = mem_node_left_rotate(node);
	}
	else if ((!node->next && node->depth > 1) ||
		(node->next && node->depth > node->next->depth + 2)) {
		// node->prev has too much depth
		// node->prev->next is going to end up at the same depth, while
		// node->prev->prev is going to move closer to root
		if (!node->prev->prev || node->prev->prev->depth < node->prev->depth - 1) {
			// Greater depth comes from node->prev->next. Double-rotation is needed.
			node->prev = mem_node_left_rotate(node->prev);
		}
		// Right-rotate
		newroot = mem_node_right_rotate(node);
	}
	else {
		// No rotation necessary
		newroot = node;
	}
	return newroot;
}

static struct mem_node *mem_node_get_page(struct mem *mem, struct mem_node *node, uint64_t addr)
{
	struct mem_node *retnode;
	if (!node) {
		// Allocate a new memory node for the page
		retnode = (struct mem_node*)malloc(sizeof(struct mem_node));
		mem_node_init(retnode, addr & ~MEM_PAGE_MASK);
		mem->node_count++;
	}
	else if (addr < node->addr) {
		retnode = mem_node_get_page(mem, node->prev, addr);
		if (!node->prev) {
		   node->prev = retnode;
		}
		else {
			node->prev = mem_node_rebalance(node->prev);
		}
		if (node->depth < node->prev->depth + 1) {
			node->depth = node->prev->depth + 1;
		}
	}
	else if (addr >= node->addr + MEM_PAGE_SIZE) {
		retnode = mem_node_get_page(mem, node->next, addr);
		if (!node->next) {
		   node->next = retnode;
		}
		else {
			node->next = mem_node_rebalance(node->next);
		}
		if (node->depth < node->next->depth + 1) {
			node->depth = node->next->depth + 1;
		}
	}
	else {
		retnode = node;
	}
	return retnode;
}

struct iter_params;
typedef int (*iter_func_t)(struct mem_node*, struct iter_params *params);
struct iter_params {
	uint64_t begin_addr, end_addr;
	iter_func_t iter_func;
	void *user_ptr;
};
static int mem_node_iterate(struct mem_node *node, struct iter_params *params)
{
	int ret;
	if (!node) {
		ret = 0;
	}
	else if (params->end_addr != 0 && node->addr >= params->end_addr) {
		// Range ends before this node
		ret = mem_node_iterate(node->prev, params);
	}
	else if (node->addr + MEM_PAGE_SIZE <= params->begin_addr) {
		// Range begins after this node
		ret = mem_node_iterate(node->next, params);
	}
	else {
		// Range contains this node
		ret = mem_node_iterate(node->prev, params);
		if (ret == 0) {
			ret = params->iter_func(node, params);
			if (ret == 0) {
				ret = mem_node_iterate(node->next, params);
			}
		}
	}
	return ret;
}

//
// Memory object
//
void mem_init(struct mem *mem, uint64_t size)
{
	memset(mem, 0, sizeof(struct mem));
	mem->size = size;
}

void mem_destroy(struct mem *mem)
{
	if (mem->root) {
		mem_node_destroy(mem->root);
		mem->root = NULL;
		mem->node_count = 0;
	}
}

static struct mem_node *mem_get_page(struct mem *mem, uint64_t addr)
{
	struct mem_node *page = mem_node_get_page(mem, mem->root, addr);
	if (mem->root == NULL) {
		mem->root = page;
	}
	else {
		mem->root = mem_node_rebalance(mem->root);
	}
	return page;
}

int mem_write8(struct mem *mem, uint64_t addr, uint8_t data)
{
	return mem_write(mem, addr, 1, &data);
}

int mem_write16(struct mem *mem, uint64_t addr, uint16_t data)
{
	uint8_t buf[2];
	put_little16(data, buf);
	return mem_write(mem, addr, 2, buf);
}

int mem_write32(struct mem *mem, uint64_t addr, uint32_t data)
{
	uint8_t buf[4];
	put_little32(data, buf);
	return mem_write(mem, addr, 4, buf);
}

int mem_write64(struct mem *mem, uint64_t addr, uint64_t data)
{
	uint8_t buf[8];
	put_little64(data, buf);
	return mem_write(mem, addr, 8, buf);
}

int mem_read8(struct mem *mem, uint64_t addr, uint8_t *data)
{
	return mem_read(mem, addr, 1, data);
}

int mem_read16(struct mem *mem, uint64_t addr, uint16_t *data)
{
	uint8_t buf[2];
	int ret = mem_read(mem, addr, 2, buf);
	if (ret == 0) *data = get_little16(buf);
	return ret;
}

int mem_read32(struct mem *mem, uint64_t addr, uint32_t *data)
{
	uint8_t buf[4];
	int ret = mem_read(mem, addr, 4, buf);
	if (ret == 0) *data = get_little32(buf);
	return ret;
}

int mem_read64(struct mem *mem, uint64_t addr, uint64_t *data)
{
	uint8_t buf[8];
	int ret = mem_read(mem, addr, 8, buf);
	if (ret == 0) *data = get_little64(buf);
	return ret;
}

int mem_load_image(struct mem *mem, uint64_t addr, uint64_t size, FILE *image_file)
{
	const uint64_t end_addr = addr + size;
	if (end_addr > mem->size || end_addr < addr) { // Check size and wrap
		return 1;
	}

	int ret = 0;
	while (addr < end_addr) {
		// Read data page by page
		uint64_t max_read = MEM_PAGE_SIZE - (addr & MEM_PAGE_MASK);
		if (max_read > end_addr - addr) {
			max_read = end_addr - addr;
		}
		struct mem_node *node = mem_get_page(mem, addr);
		size_t num_read = fread(node->data + (addr & MEM_PAGE_MASK), 1, max_read, image_file);
		if (num_read != max_read) {
			ret = 1;
			break;
		}
		addr += max_read;
	}
	return ret;
}

int mem_write(struct mem *mem, uint64_t addr, uint64_t size, const uint8_t *data)
{
	const uint64_t end_addr = addr + size;
	if (end_addr > mem->size || end_addr < addr) { // Check size and wrap
		return 1;
	}

	while (addr < end_addr) {
		// Copy data page by page
		uint64_t max_copy = MEM_PAGE_SIZE - (addr & MEM_PAGE_MASK);
		if (max_copy > end_addr - addr) {
			max_copy = end_addr - addr;
		}
		struct mem_node *node = mem_get_page(mem, addr);
		memcpy(node->data + (addr & MEM_PAGE_MASK), data, max_copy);
		data += max_copy;
		addr += max_copy;
	}
	return 0;
}

int mem_read(struct mem *mem, uint64_t addr, uint64_t size, uint8_t *data)
{
	const uint64_t end_addr = addr + size;
	if (end_addr > mem->size || end_addr < addr) { // Check size and wrap
		return 1;
	}

	while (addr < end_addr) {
		// Copy data page by page
		uint64_t max_copy = MEM_PAGE_SIZE - (addr & MEM_PAGE_MASK);
		if (max_copy > end_addr - addr) {
			max_copy = end_addr - addr;
		}
		struct mem_node *node = mem_get_page(mem, addr);
		memcpy(data, node->data + (addr & MEM_PAGE_MASK), max_copy);
		data += max_copy;
		addr += max_copy;
	}
	return 0;
}

static int print_hex_iter_func(struct mem_node *node, struct iter_params *params)
{
	FILE *hex_file = (FILE*)params->user_ptr;

	uint64_t addr = node->addr;
	if (addr < params->begin_addr) {
		addr = params->begin_addr;
	}
	addr &= ~(uint64_t)0xf;

	uint64_t stop_addr = node->addr + MEM_PAGE_SIZE;
	if (params->end_addr != 0 && stop_addr > params->end_addr) {
		stop_addr = params->end_addr;
		stop_addr = (stop_addr + 15) & ~(uint64_t)0xf;
	}

	int ret = 0;
	for (; ret == 0 && addr < stop_addr; addr += 16) {
		int i;
		for (i = 0; i < 16; i++) {
			if (node->data[(addr + i) & MEM_PAGE_MASK]) {
				break;
			}
		}
		if (i == 16) { // Don't print rows that are all zero
			continue;
		}

		// Print row address
		ret = fprintf(hex_file, "%016"PRIx64":", addr);
		if (ret < 0) break;
		// Print row data
		for (i = 0; i < 16; i++) {
			ret = fprintf(hex_file, " %02x", node->data[(addr + i) & MEM_PAGE_MASK]);
			if (ret < 0) break;
		}
		if (i == 16) {
			ret = fprintf(hex_file, "\n");
			if (ret >= 0) ret = 0;
		}
	}
	return ret;
}

int mem_dump_hex(struct mem *mem, uint64_t addr, uint64_t size, FILE *hex_file)
{
	struct iter_params params;
	params.begin_addr = addr;
	params.end_addr = addr + size;
	params.iter_func = print_hex_iter_func;
	params.user_ptr = hex_file;
	return mem_node_iterate(mem->root, &params);
}
