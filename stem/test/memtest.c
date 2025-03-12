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

	//
	// Test initial conditions
	//
	assert(mem.node_count == 0);
	assert(mem.size == TEST_MEM_SIZE);
	assert(mem.root == NULL);

	// Get current time
	struct timeval tv;
	int ret = gettimeofday(&tv, NULL);
	if (ret != 0) {
		fprintf(stderr, "error: failed to get current time\n");
		return 1;
	}

	// Seed random number generator with current time in microseconds
	srandom(tv.tv_usec + tv.tv_sec * 1000000);

	//
	// Test tree balancing
	//
	uint8_t temp_u8;
	enum { BALANCE_CYCLES = TEST_MEM_SIZE / MEM_PAGE_SIZE };
	// Generate an array of sorted page indices
	int *spi = (int*)malloc(sizeof(int) * BALANCE_CYCLES);
	for (int i = 0; i < BALANCE_CYCLES; i++) {
		spi[i] = i;
	}
	// Generate an array of unsorted page indices
	int *upi = (int*)malloc(sizeof(int) * BALANCE_CYCLES);
	for (int i = 0; i < BALANCE_CYCLES; i++) {
		// Pick a random index from the sorted array
		int j = random() % (BALANCE_CYCLES - i);
		upi[i] = spi[j];
		for (; j < BALANCE_CYCLES - i - 1; j++) {
			spi[j] = spi[j + 1];
		}
	}
	free(spi);
	for (int i = 0; i < BALANCE_CYCLES; i++) {
		temp_u8 = 1;
		ret = mem_read8(&mem, upi[i] * MEM_PAGE_SIZE, &temp_u8);
		assert(mem.root != NULL && temp_u8 == 0);
		int maxd = -1, prevd = -1, nextd = -1;
		if (mem.root->prev) {
			prevd = mem.root->prev->depth;
			if (prevd > maxd) maxd = prevd;
		}
		if (mem.root->next) {
			nextd = mem.root->next->depth;
			if (nextd > maxd) maxd = nextd;
		}
		int diffd = prevd - nextd;
		assert(mem.root->depth == maxd + 1 && diffd <= 1 && diffd >= -1);
	}
	free(upi);

	mem_destroy(&mem);
	mem_init(&mem, TEST_MEM_SIZE);

	// Test writing and reading random values
	uint64_t addr;
	enum { NUM_CYCLES = 1000 };
	for (int i = 0; i < NUM_CYCLES; i++) {
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

	mem_destroy(&mem);

	return 0;
}
