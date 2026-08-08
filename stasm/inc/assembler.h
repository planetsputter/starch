// assembler.h

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "bstr.h"
#include "bmap.h"
#include "expr.h"
#include "label.h"
#include "stub.h"
#include "token.h"

// Assembler
struct assembler {
	int state;
	FILE *outfile;
	struct bmap *defs; // Symbol definitions
	int code; // Current opcode
	struct token *word1, *word2, *include;

	struct expr_parser ep; // Expression parser

	// Output stub file sections
	int sec_count;
	struct stub_sec curr_sec; // The current section

	struct label_rec *label_recs; // Label records
};

// Initializes the assembler, writing to the given stub output file
void assembler_init(struct assembler*, FILE *outfile);

// Destroys the given asesmbler
void assembler_destroy(struct assembler*);

// Parses and takes ownership of the given token.
// Returns zero on success.
int assembler_handle_token(struct assembler*, struct token *token);

// Returns a token containing the parsed filename string literal included
// as a result of the last parsed byte, or NULL if there is none.
// Caller takes ownership of the token.
struct token *assembler_get_include(struct assembler*);

// Indicates that the input token stream has finished.
// The current statement must be complete and all labels must be defined.
// Returns zero on success.
int assembler_finish(struct assembler*, int lineno, int charno);

// Returns the opcode with the smallest immediate value required to perform the function
// of the given opcode (or pseudo-op) with the immediate value, or -1 on error.
// Sets *oob if out of bounds.
int assembler_compact_op(int opcode, bool pseudo_op, int64_t imm_val, bool *oob);
