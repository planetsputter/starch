// util.h

#pragma once

#include <stdint.h>

// Put the given 16-bit value into the given byte array in little-endian order
void put_little16(uint16_t val, uint8_t *data);

// Put the given 32-bit value into the given byte array in little-endian order
void put_little32(uint32_t val, uint8_t *data);

// Put the given 64-bit value into the given byte array in little-endian order
void put_little64(uint64_t val, uint8_t *data);

// Returns the 16-bit value represented by the array of two bytes in little-endian order
uint16_t get_little16(const uint8_t *data);

// Returns the 32-bit value represented by the array of two bytes in little-endian order
uint32_t get_little32(const uint8_t *data);

// Returns the 64-bit value represented by the array of two bytes in little-endian order
uint64_t get_little64(const uint8_t *data);
