// smaptest.c

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "bstr.h"
#include "smap.h"

void str_free(char *str)
{
	free(str);
}

int main()
{
	struct smap smap;

	// Test with C-string
	smap_init(&smap, str_free);
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
