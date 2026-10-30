// bmap.h
//
// B-string map used by Starch projects

#pragma once

#include <stdbool.h>

#include "bstr.h"

// Structure which represents a map of B-strings
struct bmap;

// Creates a new empty string map and returns a pointer to this map
struct bmap *bmap_create();

// Deletes the given string map, calling bfree() for all its keys and values
void bmap_delete(struct bmap*);

// Looks up the given key in the given string map and sets *val to the
// corresponding value. Returns whether a matching key was found.
bool bmap_get(const struct bmap*, const bchar *key, bchar **val);

// Inserts the given key/value pair into the given string map
void bmap_insert(struct bmap*, bchar *key, bchar *val);

// Removes the node with the matching key from the tree
void bmap_remove(struct bmap*, const bchar *key);

// Iterates over all key/value pairs from leftmost to rightmost, calling the given function for each.
// If the given function returns non-zero, stops iteration.
// Returns the last return value of the given function, or zero for an empty tree.
int bmap_iter(struct bmap*, void *user_ptr, int (*iter_func)(const bchar *key, const bchar *val, void *user_ptr));
