// assembler.h

#pragma once

#include "bstr.h"
#include "tokenizer.h"
#include "utf8.h"

// Assembler
struct assembler {
	bchar *outfilename;
	struct utf8_decoder decoder;
	struct tokenizer tokenizer;
};

// Initializes the assembler, taking ownership of the given B-string filename
void assembler_init(struct assembler*, bchar *filename);

// Destroys the given asesmbler
void assembler_destroy(struct assembler*);

// Parses the given byte, setting *error to zero on success
void assembler_parse_byte(struct assembler*, byte b, int *error);

// Parses all the bytes until the null byte in the given string or until an error occurs,
// setting *error to zero on success
void assembler_parse_string(struct assembler*, const char *str, int *error);

// Sets *filename to the B-string filename included as a result of the last parsed byte,
// or NULL if there is none
void assembler_get_include(struct assembler*, bchar **filename);
