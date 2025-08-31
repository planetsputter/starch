// parser.h
//
// Parser for a Starch assembly file.

#pragma once

#include <stdint.h>
#include <stdio.h>

#include "smap.h"
#include "utf8.h"

// Parser event types
enum parser_event_type {
	PET_NONE = 0,
	PET_INST, // Instruction emitted
	PET_DATA, // Raw data
	PET_SECTION, // Begin new section
	PET_INCLUDE, // Include a file
	PET_LABEL, // A label
};

// Structure representing an event emitted by the parser
struct parser_event {
	enum parser_event_type type;
	union {
		struct { // For PET_INST
			int opcode; // Starch opcode
			bchar *imm; // B-string immediate value, or NULL for none
		} inst;
		struct { // For PET_DATA
			int len;
			uint8_t raw[8];
		} data;
		struct { // For PET_SECTION
			uint64_t addr;
			uint8_t flags;
		} sec;
		bchar *filename; // B-string, for PET_INCLUDE
		bchar *label; // B-string, for PET_LABEL
	};
};

// Initializes the parser event
void parser_event_init(struct parser_event*);

// Destroy the parser event
void parser_event_destroy(struct parser_event*);

// Prints the parser event to the given file
int parser_event_print(const struct parser_event*, FILE*);

// Structure for parsing a starch assembly file
struct parser {
	bchar *filename; // B-string filename, for error messages
	struct utf8_decoder decoder;
	uint8_t ss, ts; // Syntax state, token state
	int line, ch, tline, tch; // Current and token line and char
	char *token; // Current token
	struct parser_event event; // Current parser event
	char *defkey;
	struct smap defs; // Symbol definitions
};

// Initializes the parser, taking ownership of the given B-string filename
void parser_init(struct parser*, bchar *filename);

// Destroys the parser
void parser_destroy(struct parser*);

// Parses the given byte, possibly generating an event.
// Returns 0 on success.
int parser_parse_byte(struct parser*, byte b, struct parser_event*);

// Parses all characters in the given string.
// Returns 0 on success.
// @todo: How to handle multiple events?
int parser_parse_string(struct parser*, const char *s);

// Returns whether the parser input can terminate now
int parser_can_terminate(struct parser*);

// Finishes parsing the input data.
// Returns 0 on success.
int parser_terminate(struct parser*, struct parser_event*);
