// bmap.c

#include <string.h>
#include <stdlib.h>

#include "bintree.h"
#include "bmap.h"

// Key comparison function used for lookup and sorting
int bmap_comp_func(bintree_key_t key1, bintree_key_t key2)
{
	return bstrcmpb((const bchar*)key1, (const bchar*)key2);
}

// Key and value deletion function
void bmap_key_val_del_func(bintree_key_t keyval)
{
	bfree(keyval);
}

struct bmap *bmap_create()
{
	return (struct bmap*)bintree_new(bmap_comp_func, bmap_key_val_del_func, bmap_key_val_del_func);
}

void bmap_delete(struct bmap *map)
{
	bintree_delete((struct bintree*)map);
}

bool bmap_get(const struct bmap *map, const bchar *key, bchar **val)
{
	return bintree_get((const struct bintree*)map, (bintree_key_t)key, (bintree_val_t*)val);
}

void bmap_insert(struct bmap *map, bchar *key, bchar *val)
{
	bintree_set((struct bintree*)map, key, val);
}

void bmap_remove(struct bmap *map, const bchar *key)
{
	bintree_remove((struct bintree*)map, (bintree_key_t)key);
}

int bmap_iter(struct bmap *map, void *user_ptr, int (*iter_func)(const bchar *key, const bchar *val, void *user_ptr))
{
	return bintree_iter((struct bintree*)map, user_ptr, (int(*)(bintree_key_t, bintree_val_t, void*))iter_func);
}
