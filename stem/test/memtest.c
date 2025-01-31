// memtest.c

#include <assert.h>
#include <stdint.h>

#include "mem.c"

enum {
	TEST_MEM_SIZE = 0x40000000,
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
	uint16_t temp_u16 = 0xffff;
	ret = mem_read16(&mem, MEM_PAGE_SIZE * 2 - 1, &temp_u16);
	assert(ret == 0 && temp_u16 == 0);
	assert(mem.node_count == 4);
	assert(mem.root->depth == 2);

	return 0;
}
