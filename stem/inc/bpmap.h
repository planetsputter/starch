// bpmap.h
//
// Breakpoint map

#pragma once

#include <stdbool.h>
#include <stdint.h>

// Structure which represents a map of integer BP addresses to counts
struct bpmap;

// Creates a new empty BP map and returns a pointer to this map.
// Currently returns a NULL pointer, as this is treated equivalently
// to an empty map in all contexts.
struct bpmap *bpmap_create();

// Deletes the given BP map
void bpmap_delete(struct bpmap*);

// Looks up the given address in the given BP map and sets *count to the
// corresponding count. Returns whether a matching address was found.
bool bpmap_get(const struct bpmap*, uint64_t addr, int *count);

// Inserts the given addr/count pair into the given BP map
void bpmap_insert(struct bpmap*, uint64_t addr, int count);

// Remove the breakpoint at the given address
void bpmap_remove(struct bpmap*, uint64_t addr);

// Iterates through breakpoints, calling the given function for each.
// If the given function returns non-zero, stops iteration.
// Returns the last return value of the given function, or zero for an empty map.
int bpmap_iter(struct bpmap*, void *user_ptr, int (*iter_func)(uint64_t, int, void*));
