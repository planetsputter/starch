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
	// Test tree balancing
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
		char *val = NULL;
		ret = bmap_get(bmap, testkeys + j, &val);
		assert(ret && val != NULL && strcmp(val, testvals + j) == 0);
	}

	bmap_delete(bmap);

	return 0;
}
