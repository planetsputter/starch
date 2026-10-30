// smap.c

#include <string.h>
#include <stdlib.h>

#include "bintree.h"

// Key comparison function used for lookup and sorting
int smap_comp_func(bintree_key_t key1, bintree_key_t key2)
{
	return strcmp((const char*)key1, (const char*)key2);
}

// Key and value deletion function
void smap_key_val_del_func(bintree_key_t keyval)
{
	free(keyval);
}

struct smap *smap_create()
{
	return (struct smap*)bintree_new(smap_comp_func, smap_key_val_del_func, smap_key_val_del_func);
}

void smap_delete(struct smap *map)
{
	bintree_delete((struct bintree*)map);
}

bool smap_get(const struct smap *map, const char *key, char **val)
{
	return bintree_get((const struct bintree*)map, (bintree_key_t)key, (bintree_val_t*)val);
}

void smap_insert(struct smap *map, char *key, char *val)
{
	bintree_set((struct bintree*)map, key, val);
}

int smap_iter(struct smap *map, void *user_ptr, int (*iter_func)(const char *key, const char *val, void *user_ptr))
{
	return bintree_iter((struct bintree*)map, user_ptr, (int(*)(bintree_key_t, bintree_val_t, void*))iter_func);
}
