// tokenizer.c

#include <assert.h>
#include <ctype.h>

#include "tokenizer.h"

enum { // Tokenizer states
	TZS_DEFAULT,
	TZS_COMMENT,
	TZS_OP,
	TZS_QUOTED,
	TZS_QUOTED_ESC,
	TZS_MULTI_COMMENT,
};

// These characters are operators or begin operators.
// The list must be in numeric order.
static const char ops[] = "\n!#%&'()*+,-./;<=>?@[\\]^`{|}~";

// These characters begin two-character operators.
// The list must be in numeric order.
// Currently this list must be a subset of the ops list.
static const char begin_ops[] = "&+-/<=>|";

// Returns whether the given character is in the above list
static bool isop(ucp c)
{
	// Use binary search for efficiency
	int low = 0, high = sizeof(ops) - 2, mid;
	while (low <= high) {
		mid = (low + high) / 2;
		int comp = c - ops[mid];
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
	memset(tz, 0, sizeof(*tz));
}

void tokenizer_destroy(struct tokenizer *tz)
{
	if (tz->ctoken) {
		bfree(tz->ctoken);
		tz->ctoken = NULL;
	}
	if (tz->token1) {
		bfree(tz->token1);
		tz->token1 = NULL;
	}
	if (tz->token2) {
		bfree(tz->token2);
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
		assert(bstrlen(tz->ctoken) > 0);
		if (tz->token1) {
			assert(tz->token2 == NULL); // We should never enqueue more than two tokens
			tz->token2 = tz->ctoken;
		}
		else {
			tz->token1 = tz->ctoken;
		}
		tz->ctoken = NULL;
	}
}

void tokenizer_parse(struct tokenizer *tz, ucp c)
{
	bool again;
	do {
		again = false;
		int error = 0;
		switch (tz->state) {
		case TZS_DEFAULT:
			if (c == 0 || isspace((int)c) || isop(c) || c == '"' || c == '-' || c == '+') {
				// These characters end the current token, if any
				tokenizer_enqueue(tz);
				bool capture = true, enqueue = false;
				if (c == '"') { // Double quote begins quoted token
					tz->state = TZS_QUOTED;
				}
				else if (begins_op(c)) { // Character begins an operator
					tz->state = TZS_OP;
				}
				else if (isop(c)) { // This character is an operator
					enqueue = true;
				}
				else { // Whitespace a null characters end tokens but do not begin tokens
					capture = false;
				}
				if (capture) {
					tz->ctoken = bstrdupu(&c, 1, &error);
				}
				if (enqueue) {
					tokenizer_enqueue(tz);
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
				tz->state = TZS_DEFAULT;
				again = true;
			}
			break;
		case TZS_OP:
			tz->state = TZS_DEFAULT;
			bool combine; // Whether to combine with the next operator character
			bool enqueue = true; // Whether to enqueue token immediately
			switch (tz->ctoken[0]) {
			case '-':
			case '+':
				if (isdigit(c)) { // Sign begins literal
					combine = true;
					enqueue = false;
				}
				else { // Sign becomes its own token
					combine = false;
				}
				break;
			case '&': // Check for "&&"
			case '|': // Check for "||"
				combine = c == (ucp)tz->ctoken[0];
				break;
			case '<': // Check for "<=" or "<<"
				combine = c == '=' || c == '<';
				break;
			case '=': // Check for "=="
				combine = c == '=';
				break;
			case '>': // Check for ">=" or ">>"
				combine = c == '=' || c == '>';
				break;
			case '/':
				combine = false;
				if (c == '/') { // Begins single-line comment
					bfree(tz->ctoken);
					tz->ctoken = NULL;
					tz->state = TZS_COMMENT;
				}
				else if (c == '*') { // Begins multi-line comment
					bfree(tz->ctoken);
					tz->ctoken = NULL;
					tz->state = TZS_MULTI_COMMENT;
				}
				break;
			default:
				combine = false;
			}

			if (combine) { // First and second characters should be combined
				tz->ctoken = bstrcatu(tz->ctoken, &c, 1, &error);
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
		case TZS_MULTI_COMMENT:
			if (tz->ctoken) {
				if (c == '/') { // End of multi-line comment
					tz->state = TZS_DEFAULT;
				}
				bfree(tz->ctoken);
				tz->ctoken = NULL;
			}
			else if (c == '*') {
				tz->ctoken = bstrdupu(&c, 1, &error);
			}
			break;
		default:
			assert(false);
		}
		// Only possible error is character value out of range, which should
		// not happen since these characters are coming from a decoded stream
		assert(error == 0);
	} while (again);
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
