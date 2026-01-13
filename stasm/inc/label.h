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
	struct label_usage *prev;
};

// Initializes the given label usage with the given file offset and address
void label_usage_init(struct label_usage *lu, long foffset, uint64_t addr);

// Applies the given label usage to the given output stub file at the given address
int label_usage_apply(const struct label_usage *lu, FILE *outfile, uint64_t label_addr);

//
// Label record
//
struct label_rec {
	bool string_lit; // Whether this label is a string literal
	bchar *label; // B-string, either label or string literal contents
	uint64_t addr; // Only relevant if usages is NULL
	// Track usages until label is defined. Label is defined if this is NULL.
	struct label_usage *usages;
	struct label_rec *prev;
};

// Initializes the given label record with the given parameters.
// Takes ownership of the given B-string label and usages.
void label_rec_init(struct label_rec *rec, bool string_lit, bchar *label, uint64_t addr, struct label_usage *usages);

// Destroys the given label record, releasing its label and usages.
void label_rec_destroy(struct label_rec *rec);

// Returns the record for the label or string literal with the given name, or NULL if it does not exist
struct label_rec *label_rec_lookup(struct label_rec *rec, bool string_lit, const bchar *label);
