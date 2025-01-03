// bstr.h

#pragma once

char *bstr_alloc(void);
void bstr_free(char *s);
int bstr_len(char *s);
char *bstr_cat(char *dest, const char *src);
char *bstr_append(char *dest, char c);
