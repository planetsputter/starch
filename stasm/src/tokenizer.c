// tokenizer.c

#include <assert.h>
#include <ctype.h>

#include "stmsg.h"
#include "tokenizer.h"

enum { // Tokenizer states
	TZS_DEFAULT,
	TZS_COMMENT,
	TZS_OP,
	TZS_QUOTED,
	TZS_QUOTED_ESC,
	TZS_MULTI_COMMENT1,
	TZS_MULTI_COMMENT2,
};

// These characters begin two-character operators.
// The list must be in numeric order.
static const char begin_ops[] = "!&+-/<=>|";

// Returns whether the given character is an operator character
static bool isopc(ucp c)
{
	return c < 0x7f && c > ' ' && !isalnum(c) && c != '_' && c != '$' && c != ':' && c != '"' && c != '\'';
}

// Returns whether the given token string is an operator
static bool isops(const bchar *s)
{
	return isopc(s[0]) && (s[1] == '\0' || isopc(s[1]));
}

// Returns whether the given character begins an operator
static bool begins_op(ucp c)
{
	// Use binary search for efficiency
	int low = 0, high = sizeof(begin_ops) - 2, mid;
	while (low <= high) {
		mid = (low + high) / 2;
		int comp = c - begin_ops[mid];
		if (comp < 0) {
			high = mid - 1;
		}
		else if (comp > 0) {
			low = mid + 1;
		}
		else {
			return true;
		}
	}
	return false;
}

void tokenizer_init(struct tokenizer *tz)
{
	tz->state = 0;
	tz->ctoken = NULL;
	tz->token1 = NULL;
	tz->token2 = NULL;
	tz->afterval = false;
	tz->iws = 0;
}

void tokenizer_destroy(struct tokenizer *tz)
{
	if (tz->ctoken) {
		token_free(tz->ctoken);
		tz->ctoken = NULL;
	}
	if (tz->token1) {
		token_free(tz->token1);
		tz->token1 = NULL;
	}
	if (tz->token2) {
		token_free(tz->token2);
		tz->token2 = NULL;
	}
}

bool tokenizer_in_progress(struct tokenizer *tz)
{
	return tz->ctoken != NULL || tz->token1 != NULL;
}

// Enqueues the current token for emission
static void tokenizer_enqueue(struct tokenizer *tz)
{
	if (tz->ctoken) {
		assert(bstrlen(tz->ctoken->str) > 0);
		if (tz->token1) {
			assert(tz->token2 == NULL); // We should never enqueue more than two tokens
			tz->token2 = tz->ctoken;
		}
		else {
			tz->token1 = tz->ctoken;
		}

		// The current token is a value if it is not an operator
		// and is not the first token in a statement
		tz->afterval = tz->iws > 0 && !isops(tz->ctoken->str);
		// Keep track of token index within statement
		if (tz->ctoken->str[0] == ';' || tz->ctoken->str[0] == '\n') {
			tz->iws = 0;
		}
		else {
			tz->iws++;
		}
		tz->ctoken = NULL;
	}
}

int tokenizer_parse(struct tokenizer *tz, ucp c, int lineno, int charno)
{
	bool again;
	int ret = 0;
	do {
		again = false;
		int error = 0;
		switch (tz->state) {
		case TZS_DEFAULT:
			if (c == 0 || isspace((int)c) || isopc(c) || c == '"' || c == '\'') {
				// These characters end the current token, if any
				tokenizer_enqueue(tz);
				bool capture = true, enqueue = false;
				if (c == '"' || c == '\'') { // Quotation begins quoted token
					tz->state = TZS_QUOTED;
				}
				else if (begins_op(c)) { // Character begins an operator
					tz->state = TZS_OP;
				}
				else if (isopc(c) || c == '\n') { // This character is an operator or is treated like one
					enqueue = true;
				}
				else { // Whitespace and null characters end tokens but do not begin tokens
					capture = false;
				}
				if (capture) {
					tz->ctoken = token_alloc(bstrdupu(&c, 1, &error), lineno, charno);
				}
				if (enqueue) {
					tokenizer_enqueue(tz);
				}
			}
			else { // Other characters start a token or continue the current one
				if (!tz->ctoken) {
					tz->ctoken = token_alloc(balloc(), lineno, charno);
				}
				tz->ctoken->str = bstrcatu(tz->ctoken->str, &c, 1, &error);
			}
			break;
		case TZS_COMMENT:
			if (c == '\n') { // Comments terminated by newline
				tz->state = TZS_DEFAULT;
				again = true;
			}
			break;
		case TZS_OP:
			tz->state = TZS_DEFAULT;
			bool combine; // Whether to combine with the next operator character
			bool enqueue = true; // Whether to enqueue token immediately
			switch (tz->ctoken->str[0]) {
			case '-':
				if (c == '-' || c == '>') { // Check for "--" or "->"
					combine = true;
				}
				else if (!tz->afterval && isdigit(c)) { // Sign begins literal
					combine = true;
					enqueue = false;
				}
				else { // Sign becomes its own token
					combine = false;
				}
				break;
			case '+':
				if (c == '+') { // Check for "++"
					combine = true;
				}
				else if (!tz->afterval && isdigit(c)) { // Sign begins literal
					combine = true;
					enqueue = false;
				}
				else { // Sign becomes its own token
					combine = false;
				}
				break;
			case '&': // Check for "&&"
			case '|': // Check for "||"
				combine = c == (ucp)tz->ctoken->str[0];
				break;
			case '<': // Check for "<=" or "<<"
				combine = c == '=' || c == '<';
				break;
			case '=': // Check for "=="
			case '!': // Check for "!="
				combine = c == '=';
				break;
			case '>': // Check for ">=" or ">>"
				combine = c == '=' || c == '>';
				break;
			case '/':
				combine = false;
				if (c == '/') { // Begins single-line comment
					token_free(tz->ctoken);
					tz->ctoken = NULL;
					tz->state = TZS_COMMENT;
				}
				else if (c == '*') { // Begins multi-line comment
					token_free(tz->ctoken);
					tz->ctoken = NULL;
					tz->state = TZS_MULTI_COMMENT1;
				}
				break;
			default:
				combine = false;
			}

			if (combine) { // First and second characters should be combined
				tz->ctoken->str = bstrcatu(tz->ctoken->str, &c, 1, &error);
			}
			else { // First character is its own token
				again = true;
			}
			if (enqueue) {
				// Enqueue potentially combined token(s)
				tokenizer_enqueue(tz);
			}
			break;
		case TZS_QUOTED:
			if (c == '\n') { // Newline disallowed in quoted literal
				stmsgtf(SMT_ERROR, tz->ctoken->lineno, tz->ctoken->charno,
					"unexpected newline in %s literal",
					tz->ctoken->str[0] == '"' ? "string" : "character");
				ret = 1;
			}
			else {
				// Append quoted character
				tz->ctoken->str = bstrcatu(tz->ctoken->str, &c, 1, &error);
				if (c == (ucp)tz->ctoken->str[0]) { // Unescaped quotation ends quoted token
					tokenizer_enqueue(tz);
					tz->state = TZS_DEFAULT;
				}
				else if (c == '\\') { // Backslash escapes the next character
					tz->state = TZS_QUOTED_ESC;
				}
			}
			break;
		case TZS_QUOTED_ESC:
			// Append escaped character
			tz->ctoken->str = bstrcatu(tz->ctoken->str, &c, 1, &error);
			tz->state = TZS_QUOTED;
			break;
		case TZS_MULTI_COMMENT2:
			if (c == '/') {
				tz->state = TZS_DEFAULT;
				break;
			}
			tz->state = TZS_MULTI_COMMENT1;
			// Fall-through
		case TZS_MULTI_COMMENT1:
			if (c == '*') {
				tz->state = TZS_MULTI_COMMENT2;
			}
			break;
		default:
			assert(false);
		}
		// Only possible error is character value out of range, which should
		// not happen since these characters are coming from a decoded stream
		assert(error == 0);
	} while (again && !ret);

	return ret;
}

struct token *tokenizer_emit(struct tokenizer *tz)
{
	struct token *token;
	if (tz->token1) {
		token = tz->token1;
		tz->token1 = tz->token2;
		tz->token2 = NULL;
	}
	else {
		token = NULL;
	}
	return token;
}

bool tokenizer_finish(struct tokenizer *tz)
{
	if (tz->state >= TZS_QUOTED) { // Cannot end stream within a quoted token or multi-line comment
		return false;
	}
	// Finish current token, if any
	tokenizer_enqueue(tz);
	tz->state = TZS_DEFAULT;
	return true;
}
