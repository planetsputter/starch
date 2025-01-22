// bstr.h

#pragma once

// Allocates and returns a B-string
char *bstr_alloc(void);
// Deallocates the given B-string
void bstr_free(char *s);
// Efficiently returns the length of the given B-string
int bstr_len(char *s);
// Efficiently concatenates the src B-string onto the dest B-string
char *bstr_cat(char *dest, const char *src);
// Efficiently appends the given character to the dest B-string
char *bstr_append(char *dest, char c);
