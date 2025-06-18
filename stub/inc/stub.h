// stub.h
//
// Binary format for use in Starch projects.

#pragma once

#include <stdint.h>
#include <stdio.h>

enum {
	STUB_FLAG_TEXT,
};

// Section of a stub file
struct stub_sec {
	uint64_t addr;
	uint8_t flags;
	uint64_t size;
};

// Initialize the given section
void stub_sec_init(struct stub_sec*, uint64_t addr, uint8_t flags, uint64_t size);

// Stub errors
enum {
	STUB_ERROR_NONE = 0,
	STUB_ERROR_PREMATURE_EOF,
	STUB_ERROR_INVALID_HEADER,
	STUB_ERROR_INVALID_SECTION_COUNT,
	STUB_ERROR_INVALID_FILE_OFFSET,
	STUB_ERROR_GAP_DATA,
	STUB_ERROR_SEEK_ERROR,
	STUB_ERROR_INVALID_SECTION_INDEX,
	STUB_ERROR_WRITE_FAILURE,
};

// Verifies that the given file is a valid stub file.
// Returns 0 on success.
int stub_verify(FILE *file);

// Sets the maximum and actual number of sections in the given stub file.
// Seeks to the beginning of section header data.
// Returns 0 on success.
int stub_get_section_counts(FILE *file, int *maxnsec, int *nsec);

// Loads the section data from the given file at the given index into *sec.
// Seeks the file to the beginning of that section data.
// Returns 0 on success.
int stub_load_section(FILE *file, int index, struct stub_sec *sec);

// Initializes the given file as a stub file with the given maximum number of sections.
// The actual number of sections will be zero until stub_save_section() is called.
// Seeks to the beginning of section data.
// Returns 0 on success.
int stub_init(FILE *file, int maxnsec);

// Sets the address and flags for the given section in the given stub file,
// using the current file position as the section end to determine the section size.
// Updates the size member of the given section structure.
// Returns 0 on success.
int stub_save_section(FILE *file, int index, struct stub_sec *sec);
