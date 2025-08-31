// bstr.c

#include "bstr.h"

#include <stdlib.h>

// B-string allocation increment
enum { BSTR_ALLOC_INC = 64 };

struct bstr_hdr {
	size_t size, len;
};

bchar *balloc(void)
{
	// Allocate a B-string with the minimum allocation size
	struct bstr_hdr *h = malloc(sizeof(struct bstr_hdr) + BSTR_ALLOC_INC);
	h->size = BSTR_ALLOC_INC;
	h->len = 0;
	bchar *s = (bchar*)(h + 1);
	s[0] = '\0';
	return s;
}

bchar *bstrdup(const char *cstr)
{
	return bstrcat(balloc(), cstr);
}

void bfree(bchar *s)
{
	if (s) {
		free(s - sizeof(struct bstr_hdr));
	}
}

size_t bstrlen(bchar *s)
{
	return ((struct bstr_hdr*)s - 1)->len;
}

// Increase the size of the B-string beginning with the given header by the minimum increment.
// Returns a pointer to the header of the possibly reallocated B-string.
static struct bstr_hdr *bstr_inc_size(struct bstr_hdr *h)
{
	h->size += BSTR_ALLOC_INC;
	return (struct bstr_hdr*)realloc(h, sizeof(struct bstr_hdr) + h->size);
}

bchar *bstrcat(bchar *dest, const char *src)
{
	struct bstr_hdr *h = (struct bstr_hdr*)dest - 1;
	size_t i;
	for (i = h->len; ; i++, src++) {
		if (i >= h->size) {
			h = bstr_inc_size(h);
			dest = (bchar*)(h + 1);
		}
		dest[i] = *src;
		if (*src == '\0') break;
	}
	h->len = i;
	return dest;
}

bchar *bstr_append(bchar *dest, char c)
{
	struct bstr_hdr *h = (struct bstr_hdr*)dest - 1;
	dest[h->len++] = c;
	if (h->len >= h->size) {
		h = bstr_inc_size(h);
		dest = (bchar*)(h + 1);
	}
	dest[h->len] = '\0';
	return dest;
}
