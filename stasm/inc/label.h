// label.h

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "bstr.h"
#include "expr.h"

//
// Label record
//
struct label_rec {
	bool string_lit; // Whether this label is a string literal
	bchar *label; // B-string, either label or string literal contents
	bool defined; // Whether this label has been defined
	uint64_t addr; // Only relevant if label has been defined
	long fpos; // File position of label
	int si; // Section index in which label is defined
	struct label_rec *prev;
};

// Returns a pointer to a newly allocated label record initialized with the given parameters.
// Takes ownership of the given B-string label.
struct label_rec *label_rec_new(bool string_lit, bool defined, bchar *label, uint64_t addr, long fpos, int si);

// Destroys the given label record, releasing its label and usages.
void label_rec_delete(struct label_rec *rec);

// Returns the record for the label or string literal with the given name, or NULL if it does not exist
struct label_rec *label_rec_lookup(struct label_rec *rec, bool string_lit, const bchar *label);

//
// Records the use of a label
//
struct label_usage {
	long foffset; // Offset of instruction in file
	uint64_t addr; // Address of usage
	int si; // Section index in which usage occurs
	int data_len; // If non-zero, the length of the raw data
	bool pseudo_op; // Whether a pseudo-op was used in the original source
	int opcode; // The opcode (not pseudo-op) written to the file
	bool needs_apply; // Whether the usage needs to be applied
	struct expr *expr; // The expression containing the label(s)
	struct label_usage *prev;
};

// Returns a pointer to a newly allocated label usage initialized with the given parameters.
// Takes ownership of the given expression. Sets needs_apply to true.
struct label_usage *label_usage_new(long foffset, uint64_t addr, int si, int data_len, bool pseudo_op, int opcode, struct expr *e);

// Destroys and deallocates the given label usage
void label_usage_delete(struct label_usage*);

// Applies the given label usage to the given output stub file at the given address, updating the given records and other usages
// if necessary due to compaction of a pseudo-op.
int label_usage_apply(struct label_usage *lu, FILE *outfile, uint64_t label_addr, struct label_rec *recs, struct label_usage *usages);
