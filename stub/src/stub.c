// stub.c

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "stub.h"

void stub_sec_init(struct stub_sec *sec, uint64_t addr, uint8_t flags, uint64_t size)
{
	sec->addr = addr;
	sec->flags = flags;
	sec->size = size;
}

enum {
	STUB_HEADER_SIZE = 4,
	STUB_SECTION_HEADER_SIZE = 25,
};

static const uint8_t expected_hdr[STUB_HEADER_SIZE] = {
	's', 't', 'b', 0x01
};

// Get a 64-bit unsigned value from eight bytes in little-endian order
static uint64_t u64_from_little8(const uint8_t *data)
{
	return (uint64_t)data[0] | ((uint64_t)data[1] << 8) |
		((uint64_t)data[2] << 16) | ((uint64_t)data[3] << 24) |
		((uint64_t)data[4] << 32) | ((uint64_t)data[5] << 40) |
		((uint64_t)data[6] << 48) | ((uint64_t)data[7] << 56);
}

// Write the 64-bit unsigned value to eight bytes in little-endian order
static void u64_to_little8(uint64_t val, uint8_t *data)
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

// Get 32-bit signed value from four bytes in little-endian order
static int32_t i32_from_little4(const uint8_t *data)
{
	return (uint32_t)data[0] | ((uint32_t)data[1] << 8) |
		((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

static void u32_to_little4(uint32_t val, uint8_t *data)
{
	data[0] = val;
	data[1] = val >> 8;
	data[2] = val >> 16;
	data[3] = val >> 24;
}

// Checks that the file has a valid header.
// Seeks to the beginning of the section counts.
// Returns 0 on success.
static int stub_check_header(FILE *file)
{
	// Seek to beginning of file
	if (fseek(file, 0, SEEK_SET)) {
		return STUB_ERROR_SEEK_ERROR;
	}

	// Read header
	uint8_t temp_array[STUB_SECTION_HEADER_SIZE];
	size_t bc = fread(temp_array, 1, sizeof(expected_hdr), file);
	if (bc != sizeof(expected_hdr)) { // Incomplete header
		return STUB_ERROR_PREMATURE_EOF;
	}

	// Check header
	if (memcmp(temp_array, expected_hdr, sizeof(expected_hdr)) != 0) {
		return STUB_ERROR_INVALID_HEADER;
	}

	return 0;
}

int stub_verify(FILE *file)
{
	// The format of the file is as follows, with byte sizes in parenthesis:
	// header (4)
	// max number of sections (4)
	// number of sections (4)
	// section header (25):
	//     addr (8)
	//     flags (1)
	//     begin file offset (8)
	//     end file offset (8)
	// [...]
	// section data (N)
	// [...]
	// EOF

	// Check file header and section counts and seek to section headers
	int maxnsec = 0, nsec = 0;
	int ret = stub_get_section_counts(file, &maxnsec, &nsec);
	if (ret) return ret;

	// Compute beginning file data offset
	uint64_t last_efo = STUB_HEADER_SIZE + 8 + maxnsec * STUB_SECTION_HEADER_SIZE;

	// Read section headers
	size_t bc;
	uint8_t temp_array[STUB_SECTION_HEADER_SIZE];
	int i;
	for (i = 0; i < nsec; i++) {
		// Read section header
		bc = fread(temp_array, 1, STUB_SECTION_HEADER_SIZE, file);
		if (bc != STUB_SECTION_HEADER_SIZE) {
			return STUB_ERROR_PREMATURE_EOF;
		}

		// Get section data begin file offset
		uint64_t bfo = u64_from_little8(temp_array + 9);

		// Get section data end file offset
		uint64_t efo = u64_from_little8(temp_array + 17);

		// Check offsets
		if (bfo != last_efo || efo < bfo) {
			return STUB_ERROR_INVALID_FILE_OFFSET;
		}
		last_efo = efo;
	}

	// Read unused section header slots to ensure all bytes are zero
	for (; i < maxnsec; i++) {
		// Read section header
		bc = fread(temp_array, 1, STUB_SECTION_HEADER_SIZE, file);
		if (bc != STUB_SECTION_HEADER_SIZE) {
			return STUB_ERROR_PREMATURE_EOF;
		}

		// Check that data is all zeroes
		for (int j = 0; j < STUB_SECTION_HEADER_SIZE; j++) {
			if (temp_array[j] != 0) {
				return STUB_ERROR_GAP_DATA;
			}
		}
	}

	// Get file size
	long fpos;
	if (fseek(file, 0, SEEK_END) || (fpos = ftell(file)) < 0) {
		return STUB_ERROR_SEEK_ERROR;
	}

	// Last section data must end file
	if ((uint64_t)fpos != last_efo) {
		return STUB_ERROR_INVALID_FILE_OFFSET;
	}

	// Stub file is valid
	return 0;
}

int stub_get_section_counts(FILE *file, int *maxnsec, int *nsec)
{
	// First check the file header and seek to section counts
	int ret = stub_check_header(file);
	if (ret) return ret;

	// Read maximum number of sections
	uint8_t temp_array[4];
	size_t bc = fread(temp_array, 1, sizeof(temp_array), file);
	if (bc != 4) {
		return STUB_ERROR_PREMATURE_EOF;
	}

	// Check maximum number of sections
	*maxnsec = i32_from_little4(temp_array);
	if (*maxnsec <= 0) {
		return STUB_ERROR_INVALID_SECTION_COUNT;
	}

	// Read number of sections
	bc = fread(temp_array, 1, sizeof(temp_array), file);
	if (bc != 4) {
		return STUB_ERROR_PREMATURE_EOF;
	}

	// Check number of sections
	*nsec = i32_from_little4(temp_array);
	if (*nsec < 0 || *nsec > *maxnsec) {
		return STUB_ERROR_INVALID_SECTION_COUNT;
	}

	return 0;
}

int stub_load_section(FILE *file, int section, struct stub_sec *sec)
{
	if (section < 0) return STUB_ERROR_INVALID_SECTION_INDEX;

	// Check against number of sections
	int maxnsec = 0, nsec = 0;
	int ret = stub_get_section_counts(file, &maxnsec, &nsec);
	if (ret) return ret;
	if (section >= nsec) return STUB_ERROR_INVALID_SECTION_INDEX;

	// Seek to the header for this section
	ret = fseek(file, STUB_SECTION_HEADER_SIZE * section, SEEK_CUR);
	if (ret) return ret;

	// Read section header
	uint8_t temp_array[STUB_SECTION_HEADER_SIZE];
	size_t bc = fread(temp_array, 1, STUB_SECTION_HEADER_SIZE, file);
	if (bc != STUB_SECTION_HEADER_SIZE) {
		return STUB_ERROR_PREMATURE_EOF;
	}

	// Interpret section header
	uint8_t flags = temp_array[8];
	uint64_t bfo = u64_from_little8(temp_array + 9);
	uint64_t efo = u64_from_little8(temp_array + 17);
	if (bfo > efo) {
		return STUB_ERROR_INVALID_FILE_OFFSET;
	}
	sec->addr = u64_from_little8(temp_array);
	sec->flags = flags;
	sec->size = efo - bfo;

	// Seek to beginning of section data
	ret = fseek(file, bfo, SEEK_SET);

	return ret;
}

int stub_init(FILE *file, int maxnsec)
{
	if (maxnsec <= 0) {
		return STUB_ERROR_INVALID_SECTION_COUNT;
	}

	// Truncate file
	if (fflush(file) || ftruncate(fileno(file), 0)) {
		return STUB_ERROR_WRITE_FAILURE;
	}
	FILE *reopened_file = freopen(NULL, "wb+", file);
	if (!reopened_file) {
		return STUB_ERROR_WRITE_FAILURE;
	}
	file = reopened_file;

	// Extend file to include unused section headers
	int ret = ftruncate(fileno(file), STUB_HEADER_SIZE + 8 + STUB_SECTION_HEADER_SIZE * maxnsec);
	if (ret) {
		return STUB_ERROR_WRITE_FAILURE;
	}

	// Seek to beginning of file
	if (fseek(file, 0, SEEK_SET)) {
		return STUB_ERROR_SEEK_ERROR;
	}

	// Write file header
	size_t bc = fwrite(expected_hdr, 1, sizeof(expected_hdr), file);
	if (bc != sizeof(expected_hdr)) {
		return STUB_ERROR_WRITE_FAILURE;
	}

	// Write maximum number of sections
	uint8_t temp_array[4];
	u32_to_little4(maxnsec, temp_array);
	bc = fwrite(temp_array, 1, 4, file);
	if (bc != 4) {
		return STUB_ERROR_WRITE_FAILURE;
	}

	// Seek to end of file, which is beginning of data
	if (fseek(file, 0, SEEK_END)) {
		return STUB_ERROR_SEEK_ERROR;
	}

	return 0;
}

int stub_save_section(FILE *file, int index, struct stub_sec *sec)
{
	if (index < 0) {
		return STUB_ERROR_INVALID_SECTION_INDEX;
	}

	// Note the current file position
	long fpos = ftell(file);
	if (fpos < 0) {
		return STUB_ERROR_SEEK_ERROR;
	}

	// Read the number of sections.
	// We expect the number of sections to be zero initially.
	int maxnsec = 0, nsec = 0;
	int ret = stub_get_section_counts(file, &maxnsec, &nsec);
	if (ret) return ret;
	if (index > nsec || index >= maxnsec) {
		return STUB_ERROR_INVALID_SECTION_INDEX;
	}

	// Determine the efo of the previous section.
	// For section 0, this is calculated from the maximum number of sections.
	uint64_t prev_efo;
	uint8_t temp_array[STUB_SECTION_HEADER_SIZE];
	if (index == 0) {
		prev_efo = STUB_HEADER_SIZE + 8 + STUB_SECTION_HEADER_SIZE * maxnsec;
	}
	else {
		// Seek to eight bytes before the header for the given section
		if (fseek(file, STUB_HEADER_SIZE + STUB_SECTION_HEADER_SIZE * nsec, SEEK_SET)) {
			return STUB_ERROR_SEEK_ERROR;
		}

		// Read the previous efo
		size_t bc = fread(temp_array, 1, 8, file);
		if (bc != 8) {
			return STUB_ERROR_PREMATURE_EOF;
		}
		prev_efo = u64_from_little8(temp_array);
	}

	// Check the previous efo
	if (prev_efo > (uint64_t)fpos) {
		return STUB_ERROR_INVALID_FILE_OFFSET;
	}

	// Prepare section header
	u64_to_little8(sec->addr, temp_array);
	temp_array[8] = sec->flags;
	// The bfo of the current section is the efo of the previous section
	u64_to_little8(prev_efo, temp_array + 9);
	// The efo of the current section is the orginal file position
	u64_to_little8((uint64_t)fpos, temp_array + 17);

	// Write the section header
	size_t bc = fwrite(temp_array, 1, STUB_SECTION_HEADER_SIZE, file);
	if (bc != STUB_SECTION_HEADER_SIZE) {
		return STUB_ERROR_WRITE_FAILURE;
	}

	// Set the actual section size
	sec->size = (uint64_t)fpos - prev_efo;

	if (index == nsec) { // Saving this section increased the number of sections by one
		if (fseek(file, STUB_HEADER_SIZE + 4, SEEK_SET)) {
			return STUB_ERROR_SEEK_ERROR;
		}
		u32_to_little4(index + 1, temp_array);
		bc = fwrite(temp_array, 1, 4, file);
		if (bc != 4) {
			return STUB_ERROR_PREMATURE_EOF;
		}
	}

	// Seek to the original position
	if (fseek(file, fpos, SEEK_SET)) {
		return STUB_ERROR_SEEK_ERROR;
	}

	return 0;
}
