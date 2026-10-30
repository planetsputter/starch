// bintree.c
//
// Binary search tree

#include <stdlib.h>

#include "bintree.h"

struct bintree_node {
	struct bintree_node *left, *right;
	bintree_key_t key;
	bintree_val_t val;
	unsigned char depth;
};

struct bintree *bintree_new(bintree_comp_t comp_func, bintree_key_del_t key_del_func,
	bintree_val_del_t val_del_func)
{
	struct bintree *tree = (struct bintree*)malloc(sizeof(struct bintree));
	tree->root = NULL;
	tree->comp_func = comp_func;
	tree->key_del_func = key_del_func;
	tree->val_del_func = val_del_func;
	return tree;
}

static void bintree_node_delete(struct bintree *tree, struct bintree_node *node)
{
	if (node) {
		bintree_node_delete(tree, node->left);
		bintree_node_delete(tree, node->right);
		if (tree->key_del_func) tree->key_del_func(node->key);
		if (tree->val_del_func) tree->val_del_func(node->val);
		free(node);
	}
}

void bintree_delete(struct bintree *tree)
{
	bintree_node_delete(tree, tree->root);
	free(tree);
}

// Sets *val to the value for the given key under the given node if it exists.
// Returns whether the key exists.
static bool bintree_get_node(const struct bintree *tree, const struct bintree_node *node, bintree_key_t key, bintree_val_t *val)
{
	bool ret;
	if (node == NULL) {
		ret = false;
	}
	else {
		int comp = tree->comp_func(key, node->key);
		if (comp < 0) {
			ret = bintree_get_node(tree, node->left, key, val);
		}
		else if (comp > 0) {
			ret = bintree_get_node(tree, node->right, key, val);
		}
		else {
			*val = node->val;
			ret = true;
		}
	}
	return ret;
}

bool bintree_get(const struct bintree *tree, bintree_key_t key, bintree_val_t *val)
{
	return bintree_get_node(tree, tree->root, key, val);
}

// Updates the depth of the given node based on the depths of its children
static void bintree_update_depth(struct bintree_node *node)
{
	if (node->left) {
		unsigned char leftdepth = node->left->depth;
		if (node->right) {
			unsigned char rightdepth = node->right->depth;
			node->depth = leftdepth > rightdepth ? leftdepth + 1 : rightdepth + 1;
		}
		else {
			node->depth = leftdepth + 1;
		}
	}
	else {
		if (node->right) {
			node->depth = node->right->depth + 1;
		}
		else {
			node->depth = 0;
		}
	}
}

// Performs a binary tree left rotation. *node must have a non-NULL right member.
static struct bintree_node *bintree_lrotate(struct bintree_node *node)
{
	struct bintree_node *newroot = node->right;
	struct bintree_node *mid = newroot->left;
	newroot->left = node;
	node->right = mid;
	bintree_update_depth(node);
	bintree_update_depth(newroot);
	return newroot;
}

// Performs a binary tree right rotation. *node must have a non-NULL left member.
static struct bintree_node *bintree_rrotate(struct bintree_node *node)
{
	struct bintree_node *newroot = node->left;
	struct bintree_node *mid = newroot->right;
	newroot->right = node;
	node->left = mid;
	bintree_update_depth(node);
	bintree_update_depth(newroot);
	return newroot;
}

// Rebalance the tree beginning at the given node, returning the new root
static struct bintree_node *bintree_rebalance(struct bintree_node *node)
{
	struct bintree_node *newroot;
	if ((!node->left && node->depth > 1) ||
		(node->left && node->depth > node->left->depth + 2)) {
		// node->right has too much depth. node->right->left is going to end up at the same depth,
		// while node->right->right is going to move closer to root.
		if (!node->right->right || node->right->right->depth < node->right->depth - 1) {
			// Greater depth comes from node->right->left. Double-rotation is needed.
			node->right = bintree_rrotate(node->right);
		}
		// Left-rotate
		newroot = bintree_lrotate(node);
	}
	else if ((!node->right && node->depth > 1) ||
		(node->right && node->depth > node->right->depth + 2)) {
		// node->left has too much depth. node->left->right is going to end up at the same depth,
		// while node->left->left is going to move closer to root.
		if (!node->left->left || node->left->left->depth < node->left->depth - 1) {
			// Greater depth comes from node->left->right. Double-rotation is needed.
			node->left = bintree_lrotate(node->left);
		}
		// Right-rotate
		newroot = bintree_rrotate(node);
	}
	else {
		// No rotation necessary
		bintree_update_depth(node);
		newroot = node;
	}
	return newroot;
}

// Set or insert the value for the given key under the given node
static struct bintree_node *bintree_set_node(struct bintree *tree, struct bintree_node *node, bintree_key_t key, bintree_val_t val)
{
	struct bintree_node *retnode;
	if (node == NULL) { // Empty tree, allocate new node
		retnode = (struct bintree_node*)malloc(sizeof(struct bintree_node));
		retnode->left = NULL;
		retnode->right = NULL;
		retnode->key = key;
		retnode->val = val;
		retnode->depth = 0;
	}
	else { // Search for node with matching key
		int comp = tree->comp_func(key, node->key);
		if (comp < 0) {
			node->left = bintree_set_node(tree, node->left, key, val);
			if (node->depth < node->left->depth + 1) {
				node->depth = node->left->depth + 1;
			}
			retnode = bintree_rebalance(node);
		}
		else if (comp > 0) {
			node->right = bintree_set_node(tree, node->right, key, val);
			if (node->depth < node->right->depth + 1) {
				node->depth = node->right->depth + 1;
			}
			retnode = bintree_rebalance(node);
		}
		else { // This node is a match
			if (tree->val_del_func) tree->val_del_func(node->val);
			if (tree->key_del_func) tree->key_del_func(node->key);
			node->val = val;
			node->key = key;
			retnode = node;
		}
	}
	return retnode;
}

void bintree_set(struct bintree *tree, bintree_key_t key, bintree_val_t val)
{
	tree->root = bintree_set_node(tree, tree->root, key, val);
}

// Unlink the leftmost node from the given node and set *pruned to it, returning the orphaned node
static struct bintree_node *bintree_lprune(struct bintree_node *node, struct bintree_node **pruned)
{
	struct bintree_node *retnode;
	if (!node->left) {
		*pruned = node;
		retnode = node->right;
		node->right = NULL;
		node->depth = 0;
	}
	else {
		node->left = bintree_lprune(node->left, pruned);
		bintree_update_depth(node);
		retnode = bintree_rebalance(node);
	}
	return retnode;
}

// Unlink the rightmost node from the given node and set *pruned to it
static struct bintree_node *bintree_rprune(struct bintree_node *node, struct bintree_node **pruned)
{
	struct bintree_node *retnode;
	if (!node->right) {
		*pruned = node;
		retnode = node->left;
		node->left = NULL;
		node->depth = 0;
	}
	else {
		node->right = bintree_rprune(node->right, pruned);
		bintree_update_depth(node);
		retnode = bintree_rebalance(node);
	}
	return retnode;
}

// Removes any node matching the given key from under the given node, returning the new root
struct bintree_node *bintree_remove_node(struct bintree *tree, struct bintree_node *node, bintree_key_t key)
{
	struct bintree_node *retnode = NULL;
	if (node != NULL) {
		int comp = tree->comp_func(key, node->key);
		if (comp == 0) { // This key matches
			if (!node->left || (node->right && node->left->depth <= node->right->depth)) { // Right has more or equal depth
				if (node->right) {
					node->right = bintree_lprune(node->right, &retnode);
					retnode->right = node->right;
					retnode->left = node->left;
					bintree_update_depth(retnode);
					retnode = bintree_rebalance(retnode);
				}
			}
			else if (node->left) { // Left has more depth
				node->left = bintree_rprune(node->left, &retnode);
				retnode->right = node->right;
				retnode->left = node->left;
				bintree_update_depth(retnode);
				retnode = bintree_rebalance(retnode);
			}
			node->left = NULL;
			node->right = NULL;
			bintree_node_delete(tree, node);
		}
		else {
			if (comp < 0) {
				node->left = bintree_remove_node(tree, node->left, key);
			}
			else {
				node->right = bintree_remove_node(tree, node->right, key);
			}
			// Correct depth
			bintree_update_depth(node);
			retnode = bintree_rebalance(node);
		}
	}
	return retnode;
}

void bintree_remove(struct bintree *tree, bintree_key_t key)
{
	tree->root = bintree_remove_node(tree, tree->root, key);
}

static int bintree_iter_node(struct bintree_node *node, void *user_ptr, int (*iter_func)(bintree_key_t key, bintree_val_t val, void *user_ptr))
{
	int ret;
	if (node == NULL) {
		ret = 0;
	}
	else {
		ret = bintree_iter_node(node->left, user_ptr, iter_func);
		if (ret == 0) {
			ret = iter_func(node->key, node->val, user_ptr);
		}
		if (ret == 0) {
			ret = bintree_iter_node(node->right, user_ptr, iter_func);
		}
	}
	return ret;
}

int bintree_iter(struct bintree *tree, void *user_ptr, int (*iter_func)(bintree_key_t key, bintree_val_t val, void *user_ptr))
{
	return bintree_iter_node(tree->root, user_ptr, iter_func);
}
