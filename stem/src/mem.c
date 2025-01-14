// mem.c

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
	uint8_t *data; // Page data
};

static void mem_node_init(struct mem_node *node, uint64_t addr)
{
	memset(node, 0, sizeof(struct mem_node));
	node->addr = addr;
	node->data = (uint8_t*)malloc(MEM_PAGE_SIZE);
	memset(node->data, 0, MEM_PAGE_SIZE);
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
	if (node->data) {
		free(node->data);
		node->data = NULL;
	}
}

static struct mem_node *mem_node_get_page(struct mem_node *node, uint64_t addr)
{
	struct mem_node *retnode;
	if (!node) {
		// Allocate a new memory node for the page
		// @todo: increment node count, rebalance
		retnode = (struct mem_node*)malloc(sizeof(struct mem_node));
		mem_node_init(retnode, addr & ~0xfff);
	}
	else if (addr < node->addr) {
		retnode = mem_node_get_page(node->prev, addr);
		if (!node->prev) {
		   node->prev = node;
		}
	}
	else if (addr >= node->addr + MEM_PAGE_SIZE) {
		retnode = mem_node_get_page(node->next, addr);
		if (!node->next) {
		   node->next = node;
		}
	}
	else {
		retnode = node;
	}
	return retnode;
}

//
// Memory object
//
void mem_init(struct mem *m)
{
	memset(m, 0, sizeof(struct mem));
}

void mem_destroy(struct mem *m)
{
	if (m->root) {
		mem_node_destroy(m->root);
	}
}

static struct mem_node *mem_get_page(struct mem *m, uint64_t addr)
{
	struct mem_node *page = mem_node_get_page(m->root, addr);
	if (m->root == NULL) {
		m->root = page;
	}
	return page;
}

//
// IO memory constants
//
enum {
	BEGIN_IO_ADDR         = 0xfffffffffff00000,
	IO_STDOUT_ADDR        = 0xfffffffffff00000,
	IO_STDIN_ADDR         = 0xfffffffffff00001,
};

int mem_write8(struct mem *m, uint64_t addr, uint8_t data)
{
	if (addr >= BEGIN_IO_ADDR) {
		if (addr == IO_STDOUT_ADDR) {
			printf("%c", data);
			return STERR_NONE;
		}
		return STERR_BAD_ACCESS;
	}

	struct mem_node *page = mem_get_page(m, addr);
	page->data[addr & MEM_PAGE_MASK] = data;
	return STERR_NONE;
}

int mem_write16(struct mem *m, uint64_t addr, uint16_t data)
{
	if (addr > BEGIN_IO_ADDR - 2) {
		if (addr < BEGIN_IO_ADDR) {
			return STERR_BAD_ALIGN; // Write partially to IO memory
		}
		return STERR_BAD_ACCESS; // No 16-bit IO write operations currently
	}

	struct mem_node *page = mem_get_page(m, addr);
	page->data[addr & MEM_PAGE_MASK] = data;
	addr++;
	if ((addr & MEM_PAGE_MASK) == 0) {
		page = mem_get_page(m, addr);
	}
	page->data[addr & MEM_PAGE_MASK] = data >> 8;
	return STERR_NONE;
}

int mem_write32(struct mem *m, uint64_t addr, uint32_t data)
{
	if (addr > BEGIN_IO_ADDR - 4) {
		if (addr < BEGIN_IO_ADDR) {
			return STERR_BAD_ALIGN; // Write partially to IO memory
		}
		return STERR_BAD_ACCESS; // No 32-bit IO write operations currently
	}

	struct mem_node *page = mem_get_page(m, addr);
	for (uint64_t last_addr = addr + 4; ;) {
		page->data[addr & MEM_PAGE_MASK] = data;
		data >>= 8;
		addr++;
		if (addr >= last_addr) break;
		else if ((addr & MEM_PAGE_MASK) == 0) {
			page = mem_get_page(m, addr);
		}
	}
	return STERR_NONE;
}

int mem_write64(struct mem *m, uint64_t addr, uint64_t data)
{
	if (addr > BEGIN_IO_ADDR - 8) {
		if (addr < BEGIN_IO_ADDR) {
			return STERR_BAD_ALIGN; // Write partially to IO memory
		}
		return STERR_BAD_ACCESS; // No 64-bit IO write operations currently
	}

	struct mem_node *page = mem_get_page(m, addr);
	for (uint64_t last_addr = addr + 8; ;) {
		page->data[addr & MEM_PAGE_MASK] = data;
		data >>= 8;
		addr++;
		if (addr >= last_addr) break;
		else if ((addr & MEM_PAGE_MASK) == 0) {
			page = mem_get_page(m, addr);
		}
	}
	return STERR_NONE;
}

int mem_read8(struct mem *m, uint64_t addr, uint8_t *data)
{
	if (addr >= BEGIN_IO_ADDR) {
		return STERR_BAD_ACCESS; // No 8-bit IO read operations currently
	}

	struct mem_node *page = mem_get_page(m, addr);
	*data = page->data[addr & MEM_PAGE_MASK];
	return STERR_NONE;
}

int mem_read16(struct mem *m, uint64_t addr, uint16_t *data)
{
	if (addr > BEGIN_IO_ADDR - 2) {
		if (addr < BEGIN_IO_ADDR) {
			return STERR_BAD_ALIGN; // Read partially from IO memory
		}
		return STERR_BAD_ACCESS; // No 16-bit IO read operations currently
	}

	struct mem_node *page = mem_get_page(m, addr);
	uint16_t temp = 0;
	for (uint64_t last_addr = addr + 2; ;) {
		temp <<= 8;
		temp |= page->data[addr & MEM_PAGE_MASK];
		addr++;
		if (addr >= last_addr) break;
		else if ((addr & MEM_PAGE_MASK) == 0) {
			page = mem_get_page(m, addr);
		}
	}
	*data = temp;
	return STERR_NONE;
}

int mem_read32(struct mem *m, uint64_t addr, uint32_t *data)
{
	if (addr > BEGIN_IO_ADDR - 4) {
		if (addr < BEGIN_IO_ADDR) {
			return STERR_BAD_ALIGN; // Read partially from IO memory
		}
		if (addr == IO_STDIN_ADDR) {
			return getchar();
		}
		return STERR_BAD_ACCESS; // No 16-bit IO read operations currently
	}

	struct mem_node *page = mem_get_page(m, addr);
	uint32_t temp = 0;
	for (uint64_t last_addr = addr + 4; ;) {
		temp <<= 8;
		temp |= page->data[addr & MEM_PAGE_MASK];
		addr++;
		if (addr >= last_addr) break;
		else if ((addr & MEM_PAGE_MASK) == 0) {
			page = mem_get_page(m, addr);
		}
	}
	*data = temp;
	return STERR_NONE;
}

int mem_read64(struct mem *m, uint64_t addr, uint64_t *data)
{
	if (addr > BEGIN_IO_ADDR - 8) {
		if (addr < BEGIN_IO_ADDR) {
			return STERR_BAD_ALIGN; // Read partially from IO memory
		}
		return STERR_BAD_ACCESS; // No 16-bit IO read operations currently
	}

	struct mem_node *page = mem_get_page(m, addr);
	uint64_t temp = 0;
	for (uint64_t last_addr = addr + 8; ;) {
		temp <<= 8;
		temp |= page->data[addr & MEM_PAGE_MASK];
		addr++;
		if (addr >= last_addr) break;
		else if ((addr & MEM_PAGE_MASK) == 0) {
			page = mem_get_page(m, addr);
		}
	}
	*data = temp;
	return STERR_NONE;
}
