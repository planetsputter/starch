// parser.h

#pragma once

#include "utf8.h"
#include <stdio.h>

// A token is a structure used internally by the parser
struct Token;
// A string pair
struct SP;

// Parses a starch assembly file
struct Parser {
	struct utf8_decoder decoder;
	int state;
	int line, col;
	char *word;
	struct Token *docFront, *docBack;
	struct SP *symbolFront, *symbolBack;
};

void Parser_Init(struct Parser*);
void Parser_Destroy(struct Parser*);

// Parses the given byte
void Parser_ParseByte(struct Parser*, byte b, int *error);

// Returns whether the parser can terminate, which is whether the UTF8 decoder can terminate
int Parser_CanTerminate(struct Parser*);

// Finishes parsing the input data. Returns 0 on success.
int Parser_Terminate(struct Parser*);

// Writes the bytecode to the given file. Returns 0 on success.
int Parser_WriteBytecode(struct Parser*, FILE *outstream);
