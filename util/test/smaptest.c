// smaptest.c

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "bstr.h"
#include "smap.c"

enum {
	TEST_SIZE = 16
};
static char testkeys[TEST_SIZE + 1];
static char testvals[TEST_SIZE + 1];

static void str_free(char *str)
{
	free(str);
}

static void test_depth(const struct smap_node *node)
{
	int maxd = -1, prevd = -1, nextd = -1;
	if (node->prev) {
		test_depth(node->prev);
		prevd = node->prev->depth;
		if (prevd > maxd) maxd = prevd;
	}
	if (node->next) {
		test_depth(node->next);
		nextd = node->next->depth;
		if (nextd > maxd) maxd = nextd;
	}
	int diffd = prevd - nextd;
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

	// Initialize with no deallocation function so we can use string literals
	struct smap smap;
	smap_init(&smap, NULL);

	//
	// Test initial conditions
	//
	assert(smap.root == NULL);

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
	// Test tree balancing
	//
	for (int i = 0; i < TEST_SIZE * 2; i++) {
		// Insert random key and value
		int j = random() % TEST_SIZE;
		smap_insert(&smap, (char*)testkeys + j, (char*)testvals + j);
		assert(smap.root != NULL);
		// Check depth of all nodes
		test_depth(smap.root);
		// Check that value was associated with key
		assert(strcmp(smap_get(&smap, (char*)testkeys + j), (char*)testvals + j) == 0);
	}

	// Re-init for C-strings
	smap_destroy(&smap);
	assert(smap.root == NULL);
	smap_init(&smap, str_free);

	// Test with C-string
	assert(smap_get(&smap, "testkey") == NULL);
	smap_insert(&smap, strdup("testkey"), strdup("testval"));
	assert(strcmp(smap_get(&smap, "testkey"), "testval") == 0);

	// Re-init for B-strings
	smap_destroy(&smap);
	assert(smap.root == NULL);
	assert(smap_get(&smap, "testkey") == NULL);
	smap_init(&smap, bfree);

	// Test with B-string
	assert(smap_get(&smap, "testkey") == NULL);
	smap_insert(&smap, bstrdupc("testkey"), bstrdupc("testval"));
	assert(strcmp(smap_get(&smap, "testkey"), "testval") == 0);
	smap_destroy(&smap);
	assert(smap_get(&smap, "testkey") == NULL);

	return 0;
}
