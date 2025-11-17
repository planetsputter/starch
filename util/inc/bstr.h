// bstr.h

#pragma once

#include <stddef.h>

#include "utf8.h"

// Character type used to identify B-strings
typedef char bchar;

// Allocates and returns a B-string
bchar *balloc(void);

// Returns a B-string duplicate of the given C-string
bchar *bstrdupc(const char*);

// Returns a B-string duplicate of the given B-string
bchar *bstrdupb(const bchar*);

// Creates a UTF-8 B-string duplicate of the given character array.
// If an encoding error occurs, sets *error and returns NULL.
// Otherwise sets *error to 0 and returns the B-string.
bchar *bstrdupu(const ucp *ca, size_t cc, int *error);

// Deallocates the given B-string
void bfree(bchar *s);

// Efficiently returns the length of the given B-string
size_t bstrlen(const bchar *s);

// Efficiently concatenates the src C-string onto the dest B-string.
// Returns the possibly reallocated dest B-string.
bchar *bstrcatc(bchar *dest, const char *src);

// Efficiently concatenates the src B-string onto the dest B-string.
// Returns the possibly reallocated dest B-string.
bchar *bstrcatb(bchar *dest, const bchar *src);

// Efficiently concatenates the src character array onto the dest B-string,
// encoding as UTF-8.
// On error, returns NULL and sets *error. Otherwise returns the possibly reallocated
// dest B-string and sets *error to 0.
bchar *bstrcatu(bchar *dest, const ucp *src, size_t cc, int *error);

// Efficiently appends the given character to the dest B-string.
// Returns the possibly reallocated dest B-string.
bchar *bstr_append(bchar *dest, char c);

// Indicates whether B-string s1 is less than, equal to, or greater than
// C-string s2 by returning an integer less than, equal to, or greater than zero.
// Note: Strings are compared using the default signedness of the char type.
int bstrcmpc(const bchar *s1, const char *s2);

// Indicates whether B-string s1 is less than, equal to, or greater than
// B-string s2 by returning an integer less than, equal to, or greater than zero.
// Note: Strings are compared using the default signedness of the char type.
int bstrcmpb(const bchar *s1, const bchar *s2);
