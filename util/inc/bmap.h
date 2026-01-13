// bmap.h
//
// B-string map used by Starch projects

#pragma once

#include "bstr.h"

// Structure which represents a map of B-strings
struct bmap;

// Creates a new empty string map and returns a pointer to this map.
// Currently returns a NULL pointer, as this is treated equivalently
// to an empty map in all contexts.
struct bmap *bmap_create();

// Deletes the given string map, calling bfree() for all its keys and values.
void bmap_delete(struct bmap*);

// Looks up the given key in the given string map and sets *val to the
// corresponding value. Returns whether a matching key was found.
int bmap_get(const struct bmap*, const bchar *key, bchar **val);

// Inserts the given key/value pair into the given string map, returning
// the modified string map.
struct bmap *bmap_insert(struct bmap*, bchar *key, bchar *val);
