// mem.c

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mem.h"
#include "starch.h"

enum {
	MEM_PAGE_SIZE = 0x1000,
	MEM_PAGE_MASK = 0xfff,
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
}

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

// Rebalance the tree beginning at the given node, returning the new root
static struct mem_node *mem_node_rebalance(struct mem_node *node)
{
	struct mem_node *newroot, *mid;
	if ((!node->prev && node->depth > 1) ||
		(node->prev && node->depth > node->prev->depth + 2)) {
		// node->next has too much depth
		// node->next->prev is going to end up at the same depth, while
		// node->next->next is going to move closer to root
		if (!node->next->next || node->next->next->depth < node->next->depth - 1) {
			// Greater depth comes from node->next->prev. Double-rotation is needed.
			newroot = node->next->prev;
			mid = newroot->next;
			newroot->next = node->next;
			node->next->prev = mid;
			node->next = newroot;
			mem_node_update_depth(newroot->next);
			mem_node_update_depth(newroot);
		}
		newroot = node->next;
		mid = newroot->prev;
		newroot->prev = node;
		node->next = mid;
		mem_node_update_depth(node);
		mem_node_update_depth(newroot);
	}
	else if ((!node->next && node->depth > 1) ||
		(node->next && node->depth > node->next->depth + 2)) {
		// node->prev has too much depth
		// node->prev->next is going to end up at the same depth, while
		// node->prev->prev is going to move closer to root
		if (!node->prev->prev || node->prev->prev->depth < node->prev->depth - 1) {
			// Greater depth comes from node->prev->next. Double-rotation is needed.
			newroot = node->prev->next;
			mid = newroot->prev;
			newroot->prev = node->prev;
			node->prev->next = mid;
			node->prev = newroot;
			mem_node_update_depth(newroot->prev);
			mem_node_update_depth(newroot);
		}
		newroot = node->prev;
		mid = newroot->next;
		newroot->next = node;
		node->prev = mid;
		mem_node_update_depth(node);
		mem_node_update_depth(newroot);
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
	if (addr >= mem->size) {
		return STERR_BAD_ADDR;
	}

	struct mem_node *page = mem_get_page(mem, addr);
	page->data[addr & MEM_PAGE_MASK] = data;
	return STERR_NONE;
}

int mem_write16(struct mem *mem, uint64_t addr, uint16_t data)
{
	if (addr >= mem->size - 1) {
		return STERR_BAD_ADDR;
	}

	struct mem_node *page = mem_get_page(mem, addr);
	page->data[addr & MEM_PAGE_MASK] = data;
	addr++;
	if ((addr & MEM_PAGE_MASK) == 0) {
		page = mem_get_page(mem, addr);
	}
	page->data[addr & MEM_PAGE_MASK] = data >> 8;
	return STERR_NONE;
}

int mem_write32(struct mem *mem, uint64_t addr, uint32_t data)
{
	if (addr >= mem->size - 3) {
		return STERR_BAD_ADDR;
	}

	struct mem_node *page = mem_get_page(mem, addr);
	for (uint64_t last_addr = addr + 4; ;) {
		page->data[addr & MEM_PAGE_MASK] = data;
		data >>= 8;
		addr++;
		if (addr >= last_addr) break;
		if ((addr & MEM_PAGE_MASK) == 0) {
			page = mem_get_page(mem, addr);
		}
	}
	return STERR_NONE;
}

int mem_write64(struct mem *mem, uint64_t addr, uint64_t data)
{
	if (addr >= mem->size - 7) {
		return STERR_BAD_ADDR;
	}

	struct mem_node *page = mem_get_page(mem, addr);
	for (uint64_t last_addr = addr + 8; ;) {
		page->data[addr & MEM_PAGE_MASK] = data;
		data >>= 8;
		addr++;
		if (addr >= last_addr) break;
		if ((addr & MEM_PAGE_MASK) == 0) {
			page = mem_get_page(mem, addr);
		}
	}
	return STERR_NONE;
}

int mem_read8(struct mem *mem, uint64_t addr, uint8_t *data)
{
	if (addr >= mem->size) {
		return STERR_BAD_ADDR;
	}

	struct mem_node *page = mem_get_page(mem, addr);
	*data = page->data[addr & MEM_PAGE_MASK];
	return STERR_NONE;
}

int mem_read16(struct mem *mem, uint64_t addr, uint16_t *data)
{
	if (addr >= mem->size - 1) {
		return STERR_BAD_ADDR;
	}

	struct mem_node *page = mem_get_page(mem, addr);
	uint16_t temp = 0;
	for (int i = 0; ;) {
		temp |= page->data[addr & MEM_PAGE_MASK] << (8 * i);
		if (++i >= 2) break;
		if ((++addr & MEM_PAGE_MASK) == 0) {
			page = mem_get_page(mem, addr);
		}
	}
	*data = temp;
	return STERR_NONE;
}

int mem_read32(struct mem *mem, uint64_t addr, uint32_t *data)
{
	if (addr >= mem->size - 3) {
		return STERR_BAD_ADDR;
	}

	struct mem_node *page = mem_get_page(mem, addr);
	uint32_t temp = 0;
	for (int i = 0; ;) {
		temp |= page->data[addr & MEM_PAGE_MASK] << (8 * i);
		if (++i >= 4) break;
		if ((++addr & MEM_PAGE_MASK) == 0) {
			page = mem_get_page(mem, addr);
		}
	}
	*data = temp;
	return STERR_NONE;
}

int mem_read64(struct mem *mem, uint64_t addr, uint64_t *data)
{
	if (addr >= mem->size - 7) {
		return STERR_BAD_ADDR;
	}

	struct mem_node *page = mem_get_page(mem, addr);
	uint64_t temp = 0;
	for (int i = 0; ;) {
		temp |= (uint64_t)page->data[addr & MEM_PAGE_MASK] << (8 * i);
		if (++i >= 8) break;
		if ((++addr & MEM_PAGE_MASK) == 0) {
			page = mem_get_page(mem, addr);
		}
	}
	*data = temp;
	return STERR_NONE;
}

int mem_load_image(struct mem *mem, uint64_t addr, uint64_t size, FILE *image_file)
{
	uint64_t end_addr = addr + size;
	int ret;
	struct mem_node *node = NULL;
	for (; addr < end_addr && (ret = fgetc(image_file)) != EOF; addr++) {
		if (node == NULL || (addr & MEM_PAGE_MASK) == 0) {
			node = mem_get_page(mem, addr);
		}
		node->data[addr & MEM_PAGE_MASK] = ret;
	}
	return feof(image_file) ? 1 : 0;
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
		ret = fprintf(hex_file, "%016lx:", addr);
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
