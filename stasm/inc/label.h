// label.h

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "bstr.h"

//
// Records the use of an undefined label
//
struct label_usage {
	long foffset; // Offset of instruction in file
	uint64_t addr; // Address of usage
	int si; // Section index in which usage occurs
	int data_len; // If non-zero, the length of the raw data label
	bool pseudo_op; // Whether a pseudo-op was used in the original source
	int opcode; // The opcode (not pseudo-op) written to the file
	bool needs_apply; // Whether the label needs to be applied
	struct label_usage *prev;
};

// Initializes the given label usage with the given file offset, address, section index, raw data length, pseudo-op status, and opcode.
// Sets needs_apply to true.
void label_usage_init(struct label_usage *lu, long foffset, uint64_t addr, int si, int data_len, bool pseudo_op, int opcode);

// Applies the given label usage to the given output stub file at the given address, updating the given records
// if necessary due to compaction of a pseudo-op.
struct label_rec;
int label_usage_apply(struct label_usage *lu, FILE *outfile, uint64_t label_addr, struct label_rec *recs);

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
	struct label_usage *usages; // Label usages list
	struct label_rec *prev;
};

// Initializes the given label record with the given parameters.
// Takes ownership of the given B-string label and usages.
void label_rec_init(struct label_rec *rec, bool string_lit, bool defined, bchar *label, uint64_t addr, long fpos, int si, struct label_usage *usages);

// Destroys the given label record, releasing its label and usages.
void label_rec_destroy(struct label_rec *rec);

// Returns the record for the label or string literal with the given name, or NULL if it does not exist
struct label_rec *label_rec_lookup(struct label_rec *rec, bool string_lit, const bchar *label);
