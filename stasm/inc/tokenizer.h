// tokenizer.h

#pragma once

#include <stdbool.h>

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

// Returns whether a token is being constructed or unemitted
bool tokenizer_in_progress(struct tokenizer*);

// Parses the given character
void tokenizer_parse(struct tokenizer*, ucp c);

// Sets *token to an emitted token, or to NULL if there are no more tokens to emit.
// Call until it sets *token to NULL to emit all tokens for each parsed character.
void tokenizer_emit(struct tokenizer*, bchar **token);

// Finishes the input stream, possibly emitting tokens.
// Returns whether the input stream could be finished successfully.
// The input stream cannot be finished successfully within a quoted token.
bool tokenizer_finish(struct tokenizer*);
