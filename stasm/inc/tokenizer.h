// tokenizer.h

#pragma once

#include "bstr.h"

struct tokenizer {
	int state;
	// Current token and two enqueued tokens
	bchar *ctoken, *token1, *token2;
};

// Initializes the tokenizer
void tokenizer_init(struct tokenizer*);

// Destroys the tokenizer and any unemitted tokens
void tokenizer_destroy(struct tokenizer*);

// Parses the given character
void tokenizer_parse(struct tokenizer*, ucp c);

// Sets *token to an emitted token, or to NULL if there are no more tokens to emit.
// Call until it sets *token to NULL to emit all tokens for each parsed character.
void tokenizer_emit(struct tokenizer*, bchar **token);

// Finishes the input stream, possibly emitting tokens
void tokenizer_finish(struct tokenizer*);
