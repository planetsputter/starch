// bstr.h

#pragma once

#include <stddef.h>

// Character type used to identify B-strings
typedef char bchar;

// Allocates and returns a B-string
bchar *balloc(void);

// Returns a B-string duplicate of the given C-string
bchar *bstrdup(const char*);

// Deallocates the given B-string
void bfree(bchar *s);

// Efficiently returns the length of the given B-string
size_t bstrlen(bchar *s);

// Efficiently concatenates the src C-string onto the dest B-string
// Returns the possibly reallocated string dest B-string.
bchar *bstrcat(bchar *dest, const char *src);

// Efficiently appends the given character to the dest B-string.
// Returns the possibly reallocated B-string.
bchar *bstr_append(bchar *dest, char c);
