// tokenizer.c

#include <assert.h>
#include <ctype.h>
#include <stdbool.h>

#include "tokenizer.h"

enum { // Tokenizer states
	TZS_DEFAULT,
	TZS_COMMENT,
	TZS_QUOTED,
	TZS_QUOTED_ESC,
};

// Single-character operators in numeric order
static const char *scos = "\n!#%&'()*+,-./:<=>?@[\\]^`{|}~";

// Returns whether the given character is a single-character operator
static bool is_sco(ucp c)
{
	// @todo: Since operators are sorted, could do a binary search here
	for (size_t i = 0; i < sizeof(scos) - 1; i++) {
		if ((ucp)scos[i] == c) return true;
	}
	return false;
}

void tokenizer_init(struct tokenizer *tz)
{
	memset(tz, 0, sizeof(*tz));
}

void tokenizer_destroy(struct tokenizer *tz)
{
	if (tz->ctoken) bfree(tz->ctoken);
	if (tz->token1) bfree(tz->token1);
	if (tz->token2) bfree(tz->token2);
	tokenizer_init(tz);
}

// Enqueues the current token for emission
static void tokenizer_enqueue(struct tokenizer *tz)
{
	if (tz->token1) {
		assert(tz->token2 == NULL); // We should never enqueue more than two tokens
		tz->token2 = tz->ctoken;
	}
	else {
		tz->token1 = tz->ctoken;
	}
	tz->ctoken = NULL;
}

void tokenizer_parse(struct tokenizer *tz, ucp c)
{
	int error = 0;
	switch (tz->state) {
	case TZS_DEFAULT:
		if (c == 0 || isspace((int)c) || is_sco(c) || c == '"' || c == ';') {
			// These characters end the current token, if any
			tokenizer_enqueue(tz);
			if (is_sco(c)) { // SCOs get their own token
				tz->ctoken = bstrdupu(&c, 1, &error);
				tokenizer_enqueue(tz);
			}
			else if (c == '"') { // Double quote begins quoted token
				tz->ctoken = bstrdupu(&c, 1, &error);
				tz->state = TZS_QUOTED;
			}
			else if (c == ';') { // Semicolon begins comment
				tz->state = TZS_COMMENT;
			}
		}
		else { // Other characters start a token or continue the current one
			if (!tz->ctoken) {
				tz->ctoken = balloc();
			}
			tz->ctoken = bstrcatu(tz->ctoken, &c, 1, &error);
		}
		break;
	case TZS_COMMENT:
		if (c == '\n') { // Comments terminated by newline
			tz->ctoken = bstrdupu(&c, 1, &error);
			assert(error == 0);
		}
		break;
	case TZS_QUOTED:
		// Append quoted character
		tz->ctoken = bstrcatu(tz->ctoken, &c, 1, &error);
		if (c == '"') { // Unescaped double quote ends quoted token
			tokenizer_enqueue(tz);
			tz->state = TZS_DEFAULT;
		}
		else if (c == '\\') { // Backslash escapes the next character
			tz->state = TZS_QUOTED_ESC;
		}
		break;
	case TZS_QUOTED_ESC:
		// Append escaped character
		tz->ctoken = bstrcatu(tz->ctoken, &c, 1, &error);
		tz->state = TZS_QUOTED;
		break;
	default:
		assert(false);
	}
	// Only possible error is character value out of range, which should
	// not happen since these characters are coming from a decoded stream
	assert(error == 0);
}

void tokenizer_emit(struct tokenizer *tz, bchar **token)
{
	if (tz->token1) {
		*token = tz->token1;
		tz->token1 = tz->token2;
		tz->token2 = NULL;
	}
	else {
		*token = NULL;
	}
}

void tokenizer_finish(struct tokenizer *tz)
{
	// Stream end is considered to end the current line
	tokenizer_parse(tz, '\n');
}
