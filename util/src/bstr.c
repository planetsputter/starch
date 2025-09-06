// bstr.c

#include "bstr.h"

#include <stdlib.h>
#include <string.h>

// B-string allocation increment. Used to prevent excessive reallocations.
enum { BSTR_ALLOC_INC = 64 };

struct bstr_hdr {
	size_t len; // Length of string
};

// Returns the total size of the allocation for the given string length including header size
static size_t bstr_alloc_size_for_len(size_t len)
{
	return sizeof(struct bstr_hdr) + (len / BSTR_ALLOC_INC + 1) * BSTR_ALLOC_INC;
}

// Returns the B-string header and string contents potentially reallocated for the given string length
static struct bstr_hdr *bstr_realloc_for_len(struct bstr_hdr *h, size_t len)
{
	size_t new_size = bstr_alloc_size_for_len(len);
	if (h == NULL || new_size != bstr_alloc_size_for_len(h->len)) {
		h = (struct bstr_hdr*)realloc(h, new_size);
	}
	return h;
}

bchar *balloc(void)
{
	// Allocate a B-string with the minimum allocation size
	struct bstr_hdr *h = bstr_realloc_for_len(NULL, 0);
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

bchar *bstrcatc(bchar *dest, const char *src)
{
	struct bstr_hdr *h = (struct bstr_hdr*)dest - 1;
	size_t i;
	for (i = h->len; ; src++) {
		dest[i] = *src;
		if (*src == '\0') break;
		i++;
		h = bstr_realloc_for_len(h, i);
		dest = (bchar*)(h + 1);
	}
	h->len = i;
	return dest;
}

bchar *bstrcatb(bchar *dest, const bchar *src)
{
	// Reallocate dest B-string to accomodate extra content
	size_t srclen = bstrlen(src);
	struct bstr_hdr *h = (struct bstr_hdr*)dest - 1;
	h = bstr_realloc_for_len(h, h->len + srclen);
	dest = (bchar*)(h + 1);

	// Copy src contents after dest contents
	memcpy(dest + h->len, src, srclen + 1);
	h->len += srclen; // Increase dest length
	return dest;
}

bchar *bstr_append(bchar *dest, char c)
{
	struct bstr_hdr *h = (struct bstr_hdr*)dest - 1;
	dest[h->len++] = c;
	h = bstr_realloc_for_len(h, h->len);
	dest = (bchar*)(h + 1);
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
