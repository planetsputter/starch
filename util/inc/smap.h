// smap.h
//
// String-map used by Starch projects

#pragma once

struct smap_node;

struct smap {
	// Organized as a binary search tree
	struct smap_node *root;
	// Function used to deallocate strings when necessary
	void (*dealloc)(char*);
};

// Initializes the given smap with the given deallocation function, or NULL
// if no deallocation is to be performed.
void smap_init(struct smap*, void (*dealloc)(char*));

// Destroys the given smap
void smap_destroy(struct smap*);

// Returns the value for the given key, or NULL if it does not exist
char *smap_get(struct smap*, const char *key);

// Inserts the given key-value pair into the smap, overwriting the value
// of any matching key after deleting using the provided function
void smap_insert(struct smap *smap, char *key, char *val);
