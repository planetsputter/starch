// bstr.h

#pragma once

// Allocates and returns a B-string
char *bstr_alloc(void);

// Returns a B-string duplicate of the given C-string
char *bstr_dup(const char*);

// Deallocates the given B-string
void bstr_free(char *s);

// Efficiently returns the length of the given B-string
int bstr_len(char *s);

// Efficiently concatenates the src C-string onto the dest B-string
// Returns the possibly reallocated string.
char *bstr_cat(char *dest, const char *src);

// Efficiently appends the given character to the dest B-string.
// Returns the possibly reallocated string.
char *bstr_append(char *dest, char c);
