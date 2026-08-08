// token.h

#pragma once

#include "bstr.h"

struct token {
	bchar *str; // Token string
	int lineno, charno;
};

// Returns a new token allocated with the given string, line number, and character number.
// Takes ownership of the string.
struct token *token_alloc(bchar *str, int lineno, int charno);

// Destroys the given token.
void token_free(struct token*);

// Returns a copy of the given
struct token *token_dup(struct token*);
