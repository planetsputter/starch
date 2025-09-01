// bstr.c

#include "bstr.h"

#include <stdlib.h>
#include <string.h>

// B-string allocation increment. Used to prevent excessive reallocations.
enum { BSTR_ALLOC_INC = 64 };

struct bstr_hdr {
	size_t size; // Size of allocation excluding header size
	size_t len; // Length of string
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

bchar *bstrdupc(const char *cstr)
{
	return bstrcatc(balloc(), cstr);
}

bchar *bstrdupb(const bchar *bstr)
{
	return bstrcatb(balloc(), bstr);
}

void bfree(bchar *s)
{
	if (s) {
		free(s - sizeof(struct bstr_hdr));
	}
}

size_t bstrlen(const bchar *s)
{
	return ((const struct bstr_hdr*)s - 1)->len;
}

// Increase the size of the B-string beginning with the given header by the minimum increment.
// Returns a pointer to the header of the possibly reallocated B-string.
static struct bstr_hdr *bstr_inc_size(struct bstr_hdr *h)
{
	h->size += BSTR_ALLOC_INC;
	return (struct bstr_hdr*)realloc(h, sizeof(struct bstr_hdr) + h->size);
}

bchar *bstrcatc(bchar *dest, const char *src)
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

bchar *bstrcatb(bchar *dest, const bchar *src)
{
	// Reallocate dest B-string to accomodate extra content
	size_t srclen = bstrlen(src);
	struct bstr_hdr *h = (struct bstr_hdr*)dest - 1;
	h->size = (h->len + srclen + BSTR_ALLOC_INC) / BSTR_ALLOC_INC * BSTR_ALLOC_INC;
	h = (struct bstr_hdr*)realloc(h, sizeof(struct bstr_hdr) + h->size);

	// Copy src contents after dest contents
	dest = (bchar*)(h + 1);
	memcpy(dest + h->len, src, srclen + 1);
	h->len += srclen; // Increase dest length
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

int bstrcmpc(const bchar *s1, const char *s2)
{
	// B-strings can contain null characters, so we iterate based on length
	size_t l1 = bstrlen(s1);
	size_t i;
	int ret;
	for (i = 0; i <= l1; i++) {
		ret = s1[i] - s2[i];
		if (ret || s2[i] == '\0') break;
	}
	if (ret == 0 && i < l1) {
		// We must check for the case where the strings are equal up to a null byte
		// which terminates the C-string, but the B-string continues beyond the null byte.
		// In these cases the longer B-string should sort after the shorter C-string.
		ret = s1[i + 1] + 256;
	}
	return ret;
}

int bstrcmpb(const bchar *s1, const bchar *s2)
{
	// B-strings can contain null characters, so we iterate based on length
	size_t l1 = bstrlen(s1), l2 = bstrlen(s2);
	size_t min = l1 < l2 ? l1 : l2;
	int ret = memcmp(s1, s2, min + 1);
	if (ret == 0) {
		// We must check for the case where the strings are equal up to a null byte
		// which terminates one string, but the other continues beyond the null byte.
		// In these cases the longer string should sort after the shorter one.
		if (l1 < l2) {
			ret = -s2[min + 1] - 256;
		}
		else if (l1 > l2) {
			ret = s1[min + 1] + 256;
		}
	}
	return ret;
}
