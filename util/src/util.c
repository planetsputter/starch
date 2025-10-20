// util.c

#include "util.h"

void put_little16(uint16_t val, uint8_t *data)
{
	data[0] = val;
	data[1] = val >> 8;
}

void put_little32(uint32_t val, uint8_t *data)
{
	data[0] = val;
	data[1] = val >> 8;
	data[2] = val >> 16;
	data[3] = val >> 24;
}

void put_little64(uint64_t val, uint8_t *data)
{
	data[0] = val;
	data[1] = val >> 8;
	data[2] = val >> 16;
	data[3] = val >> 24;
	data[4] = val >> 32;
	data[5] = val >> 40;
	data[6] = val >> 48;
	data[7] = val >> 56;
}

uint16_t get_little16(const uint8_t *data)
{
	return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

uint32_t get_little32(const uint8_t *data)
{
	return (uint32_t)data[0] |
		((uint32_t)data[1] << 8) |
		((uint32_t)data[2] << 16) |
		((uint32_t)data[3] << 24);
}

uint64_t get_little64(const uint8_t *data)
{
	return (uint64_t)data[0] |
		((uint64_t)data[1] << 8) |
		((uint64_t)data[2] << 16) |
		((uint64_t)data[3] << 24) |
		((uint64_t)data[4] << 32) |
		((uint64_t)data[5] << 40) |
		((uint64_t)data[6] << 48) |
		((uint64_t)data[7] << 56);
}
