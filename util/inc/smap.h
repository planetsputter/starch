// smap.h
//
// C-string map used by Starch projects

#pragma once

#include <stdbool.h>

// Structure which represents a map of C-strings
struct smap;

// Creates a new empty string map and returns a pointer to this map
struct smap *smap_create();

// Deletes the given string map, calling free() for all its keys and values
void smap_delete(struct smap*);

// Looks up the given key in the given string map and sets *val to the
// corresponding value. Returns whether a matching key was found.
bool smap_get(const struct smap*, const char *key, char **val);

// Inserts the given key/value pair into the given string map
void smap_insert(struct smap*, char *key, char *val);

// Iterates over all key/value pairs from leftmost to rightmost, calling the given function for each.
// If the given function returns non-zero, stops iteration.
// Returns the last return value of the given function, or zero for an empty tree.
int smap_iter(struct smap*, void *user_ptr, int (*iter_func)(const char *key, const char *val, void *user_ptr));
