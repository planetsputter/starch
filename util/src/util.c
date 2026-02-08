// util.c

#include <ctype.h>

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

int min_bytes_for_val(int64_t val)
{
	if (val < (int32_t)0x80000000) return 8;
	if (val < (int16_t)0x8000) return 4;
	if (val < (int8_t)0x80) return 2;
	if (val <= (uint8_t)0xff) return 1;
	if (val <= (uint16_t)0xffff) return 2;
	if (val <= (uint32_t)0xffffffff) return 4;
	return 8;
}

int lexinum_cmp(const char *a, const char *b)
{
	int comp;
	for (;;) {
		if (isdigit(*a)) {
			if (!isdigit(*b)) return 1; // a > b
			// Both *a and *b are digits. Compare numerically.
			int av = *a - '0';
			int bv = *b - '0';
			a++, b++;
			for (; isdigit(*a); a++) av = av * 10 + (*a - '0');
			for (; isdigit(*b); b++) bv = bv * 10 + (*b - '0');
			comp = av - bv;
		}
		else if (isdigit(*b)) return -1; // a < b
		else comp = (uint8_t)(*a) - (uint8_t)(*b); // Compare lexically
		if (comp != 0) return comp;
		if (*a == '\0') return 0; // a == b
		a++, b++;
	}
}
