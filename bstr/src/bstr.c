// bstr.c

#include "bstr.h"

#include <stdlib.h>

enum { BSTR_ALLOC_INC = 64 };

struct bstr_hdr {
	int size, len;
};

char *bstr_alloc(void)
{
	struct bstr_hdr *h = malloc(sizeof(struct bstr_hdr) + BSTR_ALLOC_INC);
	h->size = BSTR_ALLOC_INC;
	h->len = 0;
	char *s = (char*)(h + 1);
	s[0] = '\0';
	return s;
}

void bstr_free(char *s)
{
	free(s - sizeof(struct bstr_hdr));
}

int bstr_len(char *s)
{
	return ((struct bstr_hdr*)(s - sizeof(struct bstr_hdr)))->len;
}

static struct bstr_hdr *bstr_inc_size(struct bstr_hdr *h)
{
	h->size += BSTR_ALLOC_INC;
	return (struct bstr_hdr*)realloc(h, sizeof(struct bstr_hdr) + h->size);
}

char *bstr_cat(char *dest, const char *src)
{
	struct bstr_hdr *h = (struct bstr_hdr*)(dest - sizeof(struct bstr_hdr));
	int i;
	for (i = h->len; 1; i++, src++) {
		if (i >= h->size) {
			h = bstr_inc_size(h);
			dest = (char*)(h + 1);
		}
		dest[i] = *src;
		if (*src == '\0') break;
	}
	h->len = i;
	return dest;
}

char *bstr_append(char *dest, char c)
{
	struct bstr_hdr *h = (struct bstr_hdr*)(dest - sizeof(struct bstr_hdr));
	dest[h->len++] = c;
	if (h->len >= h->size) {
		h = bstr_inc_size(h);
		dest = (char*)(h + 1);
	}
	dest[h->len] = '\0';
	return dest;
}
