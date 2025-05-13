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
const char *testkeys = "kkkkkkkkkkkkkkkk";
const char *testvals = "vvvvvvvvvvvvvvvv";

void str_free(char *str)
{
	free(str);
}

int main()
{
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
	for (int i = 0; i < TEST_SIZE; i++) {
		// Insert random key and value
		int j = random() % TEST_SIZE;
		smap_insert(&smap, (char*)testkeys + j, (char*)testvals + j);
		assert(smap.root != NULL);
		// Check depth
		int maxd = -1, prevd = -1, nextd = -1;
		if (smap.root->prev) {
			prevd = smap.root->prev->depth;
			if (prevd > maxd) maxd = prevd;
		}
		if (smap.root->next) {
			nextd = smap.root->next->depth;
			if (nextd > maxd) maxd = nextd;
		}
		int diffd = prevd - nextd;
		assert(smap.root->depth == maxd + 1 && diffd <= 1 && diffd >= -1);
	}

	// Re-init for C-strings
	smap_destroy(&smap);
	assert(smap.root == NULL);
	smap_init(&smap, str_free);

	// Test with C-string
	assert(smap_get(&smap, "testkey") == NULL);
	smap_insert(&smap, strdup("testkey"), strdup("testval"));
	assert(smap_get(&smap, "testkey") != NULL);
	smap_destroy(&smap);
	assert(smap_get(&smap, "testkey") == NULL);

	// Test with B-string
	smap_init(&smap, bstr_free);
	assert(smap_get(&smap, "testkey") == NULL);
	smap_insert(&smap, bstr_dup("testkey"), bstr_dup("testval"));
	assert(smap_get(&smap, "testkey") != NULL);
	smap_destroy(&smap);
	assert(smap_get(&smap, "testkey") == NULL);

	return 0;
}
