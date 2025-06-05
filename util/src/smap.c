// smap.c

#include <string.h>
#include <stdlib.h>

#include "smap.h"

struct smap_node {
	char *key, *val;
	struct smap_node *prev, *next;
	int depth;
};

static void smap_node_init(struct smap_node *node, char *key, char *val)
{
	memset(node, 0, sizeof(struct smap_node));
	node->key = key;
	node->val = val;
	node->depth = 0;
}

static void smap_node_delete(struct smap *smap, struct smap_node *node)
{
	if (!node) return;
	if (smap->dealloc) {
		smap->dealloc(node->key);
		smap->dealloc(node->val);
	}
	if (node->prev) smap_node_delete(smap, node->prev);
	if (node->next) smap_node_delete(smap, node->next);
	free(node);
}

// Updates the depth of the given node based on the depths of its children
static void smap_node_update_depth(struct smap_node *node)
{
	if (node->prev) {
		int prevdepth = node->prev->depth;
		if (node->next) {
			int nextdepth = node->next->depth;
			node->depth = prevdepth > nextdepth ? prevdepth + 1 : nextdepth + 1;
		}
		else {
			node->depth = prevdepth + 1;
		}
	}
	else {
		if (node->next) {
			node->depth = node->next->depth + 1;
		}
		else {
			node->depth = 0;
		}
	}
}

// Performs a binary tree left rotation. *node must have a non-NULL next member.
static struct smap_node *smap_node_left_rotate(struct smap_node *node)
{
	struct smap_node *newroot = node->next;
	struct smap_node *mid = newroot->prev;
	newroot->prev = node;
	node->next = mid;
	smap_node_update_depth(node);
	smap_node_update_depth(newroot);
	return newroot;
}

// Performs a binary tree right rotation. *node must have a non-NULL prev member.
static struct smap_node *smap_node_right_rotate(struct smap_node *node)
{
	struct smap_node *newroot = node->prev;
	struct smap_node *mid = newroot->next;
	newroot->next = node;
	node->prev = mid;
	smap_node_update_depth(node);
	smap_node_update_depth(newroot);
	return newroot;
}

// Rebalance the tree beginning at the given node, returning the new root
static struct smap_node *smap_node_rebalance(struct smap_node *node)
{
	struct smap_node *newroot;
	if ((!node->prev && node->depth > 1) ||
		(node->prev && node->depth > node->prev->depth + 2)) {
		// node->next has too much depth
		// node->next->prev is going to end up at the same depth, while
		// node->next->next is going to move closer to root
		if (!node->next->next || node->next->next->depth < node->next->depth - 1) {
			// Greater depth comes from node->next->prev. Double-rotation is needed.
			node->next = smap_node_right_rotate(node->next);
		}
		// Left-rotate
		newroot = smap_node_left_rotate(node);
	}
	else if ((!node->next && node->depth > 1) ||
		(node->next && node->depth > node->next->depth + 2)) {
		// node->prev has too much depth
		// node->prev->next is going to end up at the same depth, while
		// node->prev->prev is going to move closer to root
		if (!node->prev->prev || node->prev->prev->depth < node->prev->depth - 1) {
			// Greater depth comes from node->prev->next. Double-rotation is needed.
			node->prev = smap_node_left_rotate(node->prev);
		}
		// Right-rotate
		newroot = smap_node_right_rotate(node);
	}
	else {
		// No rotation necessary
		newroot = node;
	}
	return newroot;
}

static char *smap_node_get(struct smap *smap, struct smap_node *node, const char *key)
{
	if (!node) return NULL;

	char *retval;
	int comp = strcmp(key, node->key);
	if (comp > 0) {
		retval = smap_node_get(smap, node->next, key);
	}
	else if (comp < 0) {
		retval = smap_node_get(smap, node->prev, key);
	}
	else {
		retval = node->val;
	}
	return retval;
}

static struct smap_node *smap_node_insert(struct smap *smap, struct smap_node *node, char *key, char *val)
{
	if (!node) { // No match found, allocate a new node
		struct smap_node *node = (struct smap_node*)malloc(sizeof(struct smap_node));
		smap_node_init(node, key, val);
		return node;
	}

	// Search for a matching node
	struct smap_node *retnode;
	int comp = strcmp(key, node->key);
	if (comp > 0) {
		retnode = smap_node_insert(smap, node->next, key, val);
		if (!node->next) {
			node->next = retnode;
		}
		else {
			node->next = smap_node_rebalance(node->next);
		}
		if (node->depth < node->next->depth + 1) {
			node->depth = node->next->depth + 1;
		}
	}
	else if (comp < 0) {
		retnode = smap_node_insert(smap, node->prev, key, val);
		if (!node->prev) {
			node->prev = retnode;
		}
		else {
			node->prev = smap_node_rebalance(node->prev);
		}
		if (node->depth < node->prev->depth + 1) {
			node->depth = node->prev->depth + 1;
		}
	}
	else {
		// This node is a match
		if (smap->dealloc) {
			smap->dealloc(node->val);
		}
		node->val = val;
		retnode = node;
	}
	return retnode;
}

void smap_init(struct smap *smap, void (*dealloc)(char*))
{
	memset(smap, 0, sizeof(struct smap));
	smap->dealloc = dealloc;
}

void smap_destroy(struct smap *smap)
{
	smap_node_delete(smap, smap->root);
	smap->root = NULL;
}

char *smap_get(struct smap *smap, const char *key)
{
	return smap_node_get(smap, smap->root, key);
}

void smap_insert(struct smap *smap, char *key, char *val)
{
	struct smap_node *node = smap_node_insert(smap, smap->root, key, val);
	if (!smap->root) {
		smap->root = node;
	}
	else {
		smap->root = smap_node_rebalance(smap->root);
	}
}
