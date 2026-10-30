// bpmap.c

#include <stdlib.h>

#include "bintree.h"
#include "bpmap.h"

// Key comparison function used for lookup and sorting
int bpmap_comp_func(bintree_key_t key1, bintree_key_t key2)
{
	return (uint64_t)key1 - (uint64_t)key2;
}

struct bpmap *bpmap_create()
{
	return (struct bpmap*)bintree_new(bpmap_comp_func, NULL, NULL);
}

void bpmap_delete(struct bpmap *map)
{
	bintree_delete((struct bintree*)map);
}

bool bpmap_get(const struct bpmap *map, uint64_t addr, int *count)
{
	bintree_val_t temp;
	bool ret = bintree_get((const struct bintree*)map, (bintree_key_t)addr, &temp);
	*count = (int)(int64_t)temp;
	return ret;
}

void bpmap_insert(struct bpmap *map, uint64_t addr, int count)
{
	bintree_set((struct bintree*)map, (bintree_key_t)addr, (bintree_val_t)(int64_t)count);
}

void bpmap_remove(struct bpmap *map, uint64_t addr)
{
	bintree_remove((struct bintree*)map, (bintree_key_t)addr);
}

// We must unfortunately use a helper struct and function to avoid casting between incompatible function types
struct bpmap_iter_helper {
	int (*iter_func)(uint64_t addr, int count, void *user_ptr);
	void *user_ptr;
};
static int bpmap_iter_func(bintree_key_t key, bintree_val_t val, void *helper)
{
	struct bpmap_iter_helper *h = (struct bpmap_iter_helper*)helper;
	return h->iter_func((uint64_t)key, (int)(int64_t)val, h->user_ptr);
}

int bpmap_iter(struct bpmap *map, void *user_ptr, int (*iter_func)(uint64_t addr, int count, void *user_ptr))
{
	// Set up the helper struct
	struct bpmap_iter_helper helper;
	helper.iter_func = iter_func;
	helper.user_ptr = user_ptr;
	return bintree_iter((struct bintree*)map, &helper, bpmap_iter_func);
}
