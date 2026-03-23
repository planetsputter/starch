// assembler.h

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "bstr.h"
#include "bmap.h"
#include "label.h"
#include "stub.h"

// Assembler
struct assembler {
	int state;
	FILE *outfile;
	struct bmap *defs; // Symbol definitions
	int code; // Current opcode
	bchar *word1, *word2, *include;
	int tlineno1, tcharno1, tlineno2, tcharno2;
	bool pret1, pret2; // Parse return values
	int64_t pval1, pval2; // Parse values

	// Output stub file sections
	int sec_count;
	struct stub_sec curr_sec; // The current section

	struct label_rec *label_recs; // Label records
};

// Initializes the assembler, writing to the given stub output file
void assembler_init(struct assembler*, FILE *outfile);

// Destroys the given asesmbler
void assembler_destroy(struct assembler*);

// Parses and takes ownership of the given B-string token.
// Returns zero on success.
int assembler_handle_token(struct assembler*, bchar *token, int tlineno, int tcharno);

// Sets *filename to the B-string filename included as a result of the last parsed byte,
// or NULL if there is none. Caller must release the B-string.
void assembler_get_include(struct assembler*, bchar **filename);

// Indicates that the input token stream has finished.
// Returns whether the input token stream could be finished successfully.
// The input stream cannot be finished successfully within a statement.
bool assembler_finish(struct assembler*);

// Returns the opcode with the smallest immediate value required to perform the function
// of the given opcode (or pseudo-op) with the immediate value, or -1 on error.
// Sets *oob if out of bounds.
int assembler_compact_op(int opcode, bool pseudo_op, int64_t imm_val, bool *oob);
