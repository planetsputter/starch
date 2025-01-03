// parser.h

#pragma once

#include "utf8.h"
#include <stdio.h>

// A token is a structure used internally by the parser
struct Token;

// Parses a starch assembly file
struct Parser {
	struct utf8_decoder decoder;
	int state;
	int line, col;
	char *word;
	struct Token *docFront, *docBack;
};

void Parser_Init(struct Parser*);
void Parser_Destroy(struct Parser*);

void Parser_ParseByte(struct Parser*, byte b, int *error);
int Parser_CanTerminate(struct Parser*);
int Parser_Terminate(struct Parser*);

int Parser_WriteBytecode(struct Parser*, FILE *outstream);
