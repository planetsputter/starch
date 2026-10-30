// bintree.h
//
// Binary search tree

#pragma once

#include <stdbool.h>

typedef void *bintree_key_t; // Key type
typedef void *bintree_val_t; // Value type
// Key comparison function used for lookup and sorting
typedef int (*bintree_comp_t)(bintree_key_t key1, bintree_key_t key2);
// Key deletion function
typedef void (*bintree_key_del_t)(bintree_key_t key);
// Value deletion function
typedef void (*bintree_val_del_t)(bintree_val_t key);

// Struct representing a node of the binary search tree
struct bintree_node;

// Struct representing an entire binary search tree
struct bintree {
	// Root node of the tree
	struct bintree_node *root;
	// Comparison function for this tree
	bintree_comp_t comp_func;
	// Key deletion function for this tree, or NULL
	bintree_key_del_t key_del_func;
	// Value deletion function for this tree, or NULL
	bintree_val_del_t val_del_func;
};

// Allocates and returns an empty bintree with the given comparison function,
// key deletion function, and value deletion function
struct bintree *bintree_new(bintree_comp_t, bintree_key_del_t, bintree_val_del_t);

// Destroys and deallocates the given bintree
void bintree_delete(struct bintree*);

// Sets *val to the value for the given key if it exists.
// Returns whether a matching key was found.
bool bintree_get(const struct bintree*, bintree_key_t key, bintree_val_t *val);

// Sets the value for the given key. Takes ownership of the given key and value
// and deletes any matching key and value.
void bintree_set(struct bintree*, bintree_key_t key, bintree_val_t val);

// Removes the node with the matching key from the tree
void bintree_remove(struct bintree *tree, bintree_key_t key);

// Iterates over all key/value pairs from leftmost to rightmost, calling the given function for each.
// If the given function returns non-zero, stops iteration.
// Returns the last return value of the given function, or zero for an empty tree.
int bintree_iter(struct bintree *tree, void *user_ptr, int (*iter_func)(bintree_key_t key, bintree_val_t val, void *user_ptr));
