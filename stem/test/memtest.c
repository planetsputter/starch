// memtest.c

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/time.h>

#include "mem.c"

enum {
	TEST_MEM_SIZE = 0x100000, // 1MiB
};

int main()
{
	struct mem mem;
	mem_init(&mem, TEST_MEM_SIZE);

	// Test initial conditions
	assert(mem.node_count == 0);
	assert(mem.size == TEST_MEM_SIZE);
	assert(mem.root == NULL);

	// Test reading memory and tree balancing
	uint8_t temp_u8 = 1;
	int ret = mem_read8(&mem, 0, &temp_u8);
	assert(ret == 0);
	assert(temp_u8 == 0);
	assert(mem.node_count == 1);
	assert(mem.root != NULL);
	assert(mem.root->depth == 0);
	assert(mem.root->addr == 0);
	temp_u8 = 1;
	ret = mem_read8(&mem, TEST_MEM_SIZE, &temp_u8);
	assert(ret == STERR_BAD_ADDR && temp_u8 == 1);
	ret = mem_read8(&mem, TEST_MEM_SIZE - 1, &temp_u8);
	assert(ret == 0 && temp_u8 == 0);
	assert(mem.node_count == 2);
	assert(mem.root->depth == 1);
	assert(mem.root->prev == NULL);
	assert(mem.root->next != NULL);
	assert(mem.root->next->depth == 0);
	assert(mem.root->next->addr == TEST_MEM_SIZE - MEM_PAGE_SIZE);
	temp_u8 = 1;
	ret = mem_read8(&mem, MEM_PAGE_SIZE, &temp_u8);
	assert(ret == 0 && temp_u8 == 0);
	assert(mem.node_count == 3);
	assert(mem.root->depth == 1);
	assert(mem.root->addr == MEM_PAGE_SIZE);
	assert(mem.root->prev != NULL);
	assert(mem.root->next != NULL);
	assert(mem.root->prev->depth == 0);
	assert(mem.root->prev->addr == 0);
	assert(mem.root->prev->prev == NULL);
	assert(mem.root->prev->next == NULL);
	assert(mem.root->next->depth == 0);
	assert(mem.root->next->addr == TEST_MEM_SIZE - MEM_PAGE_SIZE);
	assert(mem.root->next->prev == NULL);
	assert(mem.root->next->next == NULL);

	// Get current time
	struct timeval tv;
	ret = gettimeofday(&tv, NULL);
	if (ret != 0) {
		fprintf(stderr, "error: failed to get current time\n");
		return 1;
	}

	// Seed random number generator with current time in microseconds
	srandom(tv.tv_usec + tv.tv_sec * 1000000);

	// Test writing and reading random values
	enum { NUM_CYCLES = 1000 };
	for (int i = 0; i < NUM_CYCLES; i++) {
		uint64_t addr;
		// Generate random values to read and write
		uint8_t valread8, valwrite8 = random();
		uint16_t valread16, valwrite16 = random();
		uint32_t valread32, valwrite32 = random();
		uint64_t valread64, valwrite64 = random();
		// Write/read 8 bit
		addr = (uint64_t)random() % TEST_MEM_SIZE;
		ret = mem_write8(&mem, addr, valwrite8);
		assert(ret == 0);
		valread8 = ~valwrite8;
		ret = mem_read8(&mem, addr, &valread8);
		assert(ret == 0 && valread8 == valwrite8);
		// Write/read 16 bit
		addr = (uint64_t)random() % (TEST_MEM_SIZE - 1);
		ret = mem_write16(&mem, addr, valwrite16);
		assert(ret == 0);
		valread16 = ~valwrite16;
		ret = mem_read16(&mem, addr, &valread16);
		assert(ret == 0 && valread16 == valwrite16);
		// Verify little-endian
		ret = mem_read8(&mem, addr, &valread8);
		assert(ret == 0 && valread8 == (valread16 & 0xff));
		ret = mem_read8(&mem, addr + 1, &valread8);
		assert(ret == 0 && valread8 == (valread16 >> 8));
		// Write/read 32 bit
		addr = (uint64_t)random() % (TEST_MEM_SIZE - 3);
		ret = mem_write32(&mem, addr, valwrite32);
		assert(ret == 0);
		valread32 = ~valwrite32;
		ret = mem_read32(&mem, addr, &valread32);
		assert(ret == 0 && valread32 == valwrite32);
		// Verify little-endian
		ret = mem_read16(&mem, addr, &valread16);
		assert(ret == 0 && valread16 == (valread32 & 0xffff));
		ret = mem_read16(&mem, addr + 2, &valread16);
		assert(ret == 0 && valread16 == (valread32 >> 16));
		// Write/read 64 bit
		addr = (uint64_t)random() % (TEST_MEM_SIZE - 7);
		ret = mem_write64(&mem, addr, valwrite64);
		assert(ret == 0);
		valread64 = ~valwrite64;
		ret = mem_read64(&mem, addr, &valread64);
		assert(ret == 0 && valread64 == valwrite64);
		// Verify little-endian
		ret = mem_read32(&mem, addr, &valread32);
		assert(ret == 0 && valread32 == (valread64 & 0xffffffff));
		ret = mem_read32(&mem, addr + 4, &valread32);
		assert(ret == 0 && valread32 == (valread64 >> 32));
	}

	return 0;
}
