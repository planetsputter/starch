// mem.h
//
// Emulated memory for use by the Starch emulator

#pragma once

#include <inttypes.h>
#include <stdio.h>

struct mem_node;

struct mem {
	struct mem_node *root;
	uint64_t node_count, size;
};

// Initializes the given mem struct with the given size.
// Addresses must be less than size.
void mem_init(struct mem*, uint64_t size);

// Destroys the given mem struct, deallocating all its memory
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

// Load a binary image file into memory. Returns 0 on success.
int mem_load_image(struct mem*, uint64_t addr, uint64_t size, FILE *image_file);

// Write memory from a buffer into the memory object at the given address
int mem_write(struct mem*, uint64_t addr, uint64_t size, const uint8_t *data);

// Read memory from the given address in the memory object into the buffer
int mem_read(struct mem*, uint64_t addr, uint64_t size, uint8_t *data);

// Dump the given range of memory to a hex file.
// If addr and size are both zero, dumps all modified memory.
// Returns 0 on success.
int mem_dump_hex(struct mem*, uint64_t addr, uint64_t size, FILE *hex_file);
