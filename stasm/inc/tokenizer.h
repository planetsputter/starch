// tokenizer.h

#pragma once

#include <stdbool.h>

#include "token.h"

struct tokenizer {
	int state;
	// Current token and two enqueued tokens
	struct token *ctoken, *token1, *token2;
	bool afterval; // Whether last enqueued token was a value
	int iws; // Index of current token within statement
};

// Initializes the tokenizer
void tokenizer_init(struct tokenizer*);

// Destroys the tokenizer and any unemitted tokens
void tokenizer_destroy(struct tokenizer*);

// Returns whether a token is being constructed or unemitted
bool tokenizer_in_progress(struct tokenizer*);

// Parses the given character
int tokenizer_parse(struct tokenizer*, ucp c, int lineno, int charno);

// Returns an emitted token, or NULL if there are no more tokens to emit.
// Call until it returns NULL to emit all tokens for each parsed character.
// The caller takes ownership of the emitted token.
struct token *tokenizer_emit(struct tokenizer*);

// Finishes the input stream, possibly emitting tokens.
// Returns whether the input stream could be finished successfully.
// The input stream cannot be finished successfully within a quoted token.
bool tokenizer_finish(struct tokenizer*);
