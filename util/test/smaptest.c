// smaptest.c

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "bstr.h"
#include "bmap.c"
#include "smap.c"

enum {
	TEST_SIZE = 16
};
static char testkeys[TEST_SIZE + 1];
static char testvals[TEST_SIZE + 1];

static void test_depth(const struct smap *node)
{
	int maxd = -1, leftd = -1, rightd = -1;
	if (node->left) {
		test_depth(node->left);
		leftd = node->left->depth;
		if (leftd > maxd) maxd = leftd;
	}
	if (node->right) {
		test_depth(node->right);
		rightd = node->right->depth;
		if (rightd > maxd) maxd = rightd;
	}
	int diffd = leftd - rightd;
	assert(node->depth == maxd + 1 && diffd <= 1 && diffd >= -1);
}

static void test_bdepth(const struct bmap *node)
{
	if (node == NULL) return;
	int maxd = -1, leftd = -1, rightd = -1;
	if (node->left) {
		test_bdepth(node->left);
		leftd = node->left->depth;
		if (leftd > maxd) maxd = leftd;
	}
	if (node->right) {
		test_bdepth(node->right);
		rightd = node->right->depth;
		if (rightd > maxd) maxd = rightd;
	}
	int diffd = leftd - rightd;
	assert(node->depth == maxd + 1 && diffd <= 1 && diffd >= -1);
}

static int get_bcount(const struct bmap *node)
{
	if (node == NULL) return 0;
	return 1 + get_bcount(node->left) + get_bcount(node->right);
}

static const char *last_key = NULL;
static int iter_func(char *key, char *val, void *user_ptr)
{
	(void)val;
	(void)user_ptr;
	if (last_key) {
		assert(strcmp(last_key, key) < 0);
	}
	last_key = key;
	return 0;
}

static const char *last_bkey = NULL;
static int biter_func(bchar *key, bchar *val, void *user_ptr)
{
	(void)val;
	(void)user_ptr;
	if (last_bkey) {
		assert(bstrcmpb(last_bkey, key) < 0);
	}
	last_bkey = key;
	return 0;
}

int main()
{
	// Initialize test keys and values arrays
	for (int i = 0; i < TEST_SIZE; i++) {
		testkeys[i] = 'k';
		testvals[i] = 'v';
	}
	testkeys[TEST_SIZE] = '\0';
	testvals[TEST_SIZE] = '\0';

	// Get current time
	struct timeval tv;
	int ret = gettimeofday(&tv, NULL);
	if (ret != 0) {
		fprintf(stderr, "error: failed to get current time\n");
		return 1;
	}

	// Seed random number generator with current time in microseconds
	srandom(tv.tv_usec + tv.tv_sec * 1000000);

	//
	// Initialize for C-strings
	//
	struct smap *smap = smap_create();

	//
	// Test initial conditions
	//
	assert(smap == NULL);

	//
	// Test for C-strings
	//
	for (int i = 0; i < TEST_SIZE * 2; i++) {
		// Insert random key and value
		int j = random() % TEST_SIZE;
		smap = smap_insert(smap, strdup(testkeys + j), strdup(testvals + j));
		assert(smap != NULL);
		// Check depth of all nodes
		test_depth(smap);
		// Check that value was associated with key
		char *val = NULL;
		ret = smap_get(smap, testkeys + j, &val);
		assert(ret && val != NULL && strcmp(val, testvals + j) == 0);
		// Test order
		last_key = NULL;
		assert(smap_iter(smap, NULL, iter_func) == 0);
	}

	smap_delete(smap);

	//
	// Initialize for B-strings
	//
	struct bmap *bmap = bmap_create();

	//
	// Test initial conditions
	//
	assert(bmap == NULL);

	//
	// Test for B-strings
	//
	for (int i = 0; i < TEST_SIZE * 2; i++) {
		// Insert random key and value
		int j = random() % TEST_SIZE;
		bmap = bmap_insert(bmap, bstrdupc(testkeys + j), bstrdupc(testvals + j));
		assert(bmap != NULL);
		// Check depth of all nodes
		test_bdepth(bmap);
		// Check that value was associated with key
		bchar *val = NULL, *name = bstrdupc(testkeys + j);
		ret = bmap_get(bmap, name, &val);
		bfree(name);
		assert(ret && val != NULL && strcmp(val, testvals + j) == 0);
		// Test order
		last_bkey = NULL;
		assert(bmap_iter(bmap, NULL, biter_func) == 0);
	}

	//
	// Test deletion of keys in random order
	//
	for (int i = 0; i < TEST_SIZE * 2; i++) {
		// Check whether key exists
		int j = random() % TEST_SIZE;
		bchar *key = bstrdupc(testkeys + j);
		bchar *val = NULL;
		int has = bmap_get(bmap, key, &val);
		assert(has == (val != NULL));
		int count_before = get_bcount(bmap);
		// Attempt to remove key
		bmap = bmap_remove(bmap, key);
		bfree(key);
		// Check depth of all nodes
		test_bdepth(bmap);
		// Assert node removed only if key existed
		assert(get_bcount(bmap) + has == count_before);
		// Test order
		last_bkey = NULL;
		assert(bmap_iter(bmap, NULL, biter_func) == 0);
	}

	bmap_delete(bmap);

	return 0;
}
