// parser.h

#pragma once

#include <stdint.h>
#include <stdio.h>

#include "smap.h"
#include "utf8.h"

// A token is a structure used internally by the parser
struct Token;

// Parses a starch assembly file
struct parser {
	struct utf8_decoder decoder;
	uint8_t ss, ts;
	int line, ch, tline, tch; // Current and token line and char
	char *token;
	char *defkey;
	struct smap defs;
	FILE *outfile;
};

// Initializes the parser and the given output stub file
int parser_init(struct parser*, FILE *outfile);

// Destroys the parser
void parser_destroy(struct parser*);

// Parses the given byte
int parser_parse_byte(struct parser*, byte b);

// Returns whether the parser can terminate, which is whether the UTF8 decoder can terminate
int parser_can_terminate(struct parser*);

// Finishes parsing the input data and writing to the output file.
// Returns 0 on success.
int parser_terminate(struct parser*);
