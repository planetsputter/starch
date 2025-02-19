// stub.h
//
// Binary format for use in Starch projects.

#pragma once

#include <stdint.h>
#include <stdio.h>

#include "starch.h"

enum {
	STUB_FLAG_TEXT = 0,
	STUB_FLAG_DATA = 1,
	STUB_FLAG_STACK = 2,
};

// Section of a stub file
struct stub_sec {
	uint64_t addr;
	uint8_t flags;
	uint64_t size;
};

enum {
	// Begin stub errors after starch errors
	STUB_ERROR_PREMATURE_EOF = STERR_NUM_ERRORS,
	STUB_ERROR_INVALID_HEADER,
	STUB_ERROR_INVALID_SECTION_COUNT,
	STUB_ERROR_INVALID_FILE_OFFSET,
	STUB_ERROR_SEEK_ERROR,
	STUB_ERROR_INVALID_SECTION_INDEX,
	STUB_ERROR_WRITE_FAILURE,
};

// Verifies that the given file is a valid stub file.
// Returns 0 on success.
int stub_verify(FILE *file);

// Returns the number of sections in the given stub file, or negative on error.
// seeks to the beginning of section header data.
int stub_count_sections(FILE *file);

// Loads the section data from the given file at the given index into *sec.
// seeks the file to the beginning of section data.
// Returns 0 on success.
int stub_load_section(FILE *file, int index, struct stub_sec *sec);

// Initializes the given file as a stub file with the given number of sections.
// Seeks to the beginning of section data.
// Returns 0 on success.
int stub_init(FILE *file, int nsec);

// Sets the address and flags for the given section in the given stub file,
// using the current file position as the section end to determine the section size.
// Returns 0 on success.
int stub_save_section(FILE *file, int index, struct stub_sec *sec);
