// bpmap.h
//
// Breakpoint map

#pragma once

#include <stdint.h>

// Structure which represents a map of integer BP addresses to counts
struct bpmap;

// Creates a new empty BP map and returns a pointer to this map.
// Currently returns a NULL pointer, as this is treated equivalently
// to an empty map in all contexts.
struct bpmap *bpmap_create();

// Deletes the given BP map
void bpmap_delete(struct bpmap*);

// Looks up the given address in the given BP map and sets *val to the
// corresponding count. Returns whether a matching address was found.
int bpmap_get(const struct bpmap*, uint64_t addr, int *count);

// Inserts the given addr/count pair into the given BP map, returning
// the modified BP map.
struct bpmap *bpmap_insert(struct bpmap*, uint64_t addr, int count);

// Remove the breakpoint at the given address, returning the modified map
struct bpmap *bpmap_remove(struct bpmap*, uint64_t addr);
