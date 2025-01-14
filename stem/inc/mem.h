// mem.h
//
// Emulated memory for use by the Starch emulator

#pragma once

#include <inttypes.h>

struct mem_node;

struct mem {
	struct mem_node *root;
	int node_count, last_rebalance;
};

void mem_init(struct mem*);
void mem_destroy(struct mem*);

// Memory write operations. Return 0 on success.
int mem_write8(struct mem*, uint64_t addr, uint8_t data);
int mem_write16(struct mem*, uint64_t addr, uint16_t data);
int mem_write32(struct mem*, uint64_t addr, uint32_t data);
int mem_write64(struct mem*, uint64_t addr, uint64_t data);

// Memory read operations. Return 0 on success.
int mem_read8(struct mem*, uint64_t addr, uint8_t *data);
int mem_read16(struct mem*, uint64_t addr, uint16_t *data);
int mem_read32(struct mem*, uint64_t addr, uint32_t *data);
int mem_read64(struct mem*, uint64_t addr, uint64_t *data);
