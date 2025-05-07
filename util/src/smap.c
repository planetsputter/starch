// smap.c

#include <string.h>
#include <stdlib.h>

#include "smap.h"

struct smap_node {
	char *key, *val;
	struct smap_node *prev, *next;
};

static void smap_node_init(struct smap_node *node, char *key, char *val)
{
	memset(node, 0, sizeof(struct smap_node));
	node->key = key;
	node->val = val;
}

static void smap_node_delete(struct smap *smap, struct smap_node *node)
{
	if (!node) return;
	smap->dealloc(node->key);
	smap->dealloc(node->val);
	if (node->prev) smap_node_delete(smap, node->prev);
	if (node->next) smap_node_delete(smap, node->next);
	free(node);
}

static char *smap_node_get(struct smap *smap, struct smap_node *node, const char *key)
{
	if (!node) return NULL;
	int comp = strcmp(key, node->key);
	if (comp > 0) {
		return smap_node_get(smap, node->next, key);
	}
	if (comp < 0) {
		return smap_node_get(smap, node->prev, key);
	}
	return node->val;
}

static struct smap_node *smap_node_insert(struct smap *smap, struct smap_node *node, char *key, char *val)
{
	// @todo: rebalance
	if (!node) { // No match found, allocate a new node
		struct smap_node *node = (struct smap_node*)malloc(sizeof(struct smap_node));
		smap_node_init(node, key, val);
		return node;
	}

	struct smap_node *retnode;
	int comp = strcmp(key, node->key);
	if (comp > 0) {
		retnode = smap_node_insert(smap, node->next, key, val);
		if (!node->next) {
			node->next = retnode;
		}
	}
	else if (comp < 0) {
		retnode = smap_node_insert(smap, node->prev, key, val);
		if (!node->prev) {
			node->prev = retnode;
		}
	}
	else {
		// This node is a match
		smap->dealloc(node->val);
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
	if (!smap->root) smap->root = node;
}
