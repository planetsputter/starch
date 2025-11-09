// littest.c

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#include "bstr.h"
#include "lits.h"

// Single-character escape codes
struct esc_pair {
	char c;
	ucp v;
};
struct esc_pair esc_pairs[] = {
	{ '\'', '\'' },
	{ '\\', '\\' },
	{ '\"', '\"' },
	{ 'a', '\a' },
	{ 'b', '\b' },
	{ 'f', '\f' },
	{ 'n', '\n' },
	{ 'r', '\r' },
	{ 't', '\t' },
	{ 'v', '\v' },
	{ '?', '\?' },
};

int main()
{
	// Test proper single-character unescapes
	ucp c = 0;
	bchar *ts = NULL;
	int error = 0;
	for (size_t i = 0; i < sizeof(esc_pairs) / sizeof(*esc_pairs); i++) {
		ts = balloc();
		ts = bstrcatu(ts, '\'', &error);
		ts = bstrcatu(ts, '\\', &error);
		ts = bstrcatu(ts, esc_pairs[i].c, &error);
		ts = bstrcatu(ts, '\'', &error);
		c = esc_pairs[i].v + 1;
		assert(parse_char_lit(ts, &c) && c == esc_pairs[i].v);
		bfree(ts);
	}

	// Get current time
	struct timeval tv;
	int ret = gettimeofday(&tv, NULL);
	if (ret != 0) {
		fprintf(stderr, "error: failed to get current time\n");
		return 1;
	}

	// Seed random number generator with current time in microseconds
	srandom(tv.tv_usec + tv.tv_sec * 1000000);

	// Test rejection of missing escape
	ts = balloc();
	ts = bstrcatu(ts, '\'', &error);
	ts = bstrcatu(ts, '\\', &error);
	ts = bstrcatu(ts, '\'', &error);
	c = (ucp)random() % (UTF8_MAX_POINT + 1);
	ucp tc = c;
	assert(!parse_char_lit(ts, &c) && c == tc);
	bfree(ts);

	enum { TEST_CYCLES = 1000 };

	for (int i = 0; i < TEST_CYCLES; i++) {
		// Any single-character B-string is an invalid character literal

		// Generate and encode a random UTF-8 character
		c = (ucp)random() % (UTF8_MAX_POINT + 1);

		// Append to B-string
		ts = balloc();
		ts = bstrcatu(ts, c, &error);

		// Assert not valid and output unchanged
		c = (ucp)random() % (UTF8_MAX_POINT + 1);
		ucp tc = c;
		assert(!parse_char_lit(ts, &c) && c == tc);

		// Any two-character B-string is an invalid character literal

		// Append random character to B-string
		ts = bstrcatu(ts, c, &error);

		// Assert not valid and output unchanged
		c = (ucp)random() % (UTF8_MAX_POINT + 1);
		tc = c;
		assert(!parse_char_lit(ts, &c) && c == tc);

		// Any B-string with format 'x' where x is not ' or \ is a valid character literal
		if (c == '\'' || c == '\\') c++;
		bfree(ts);
		ts = balloc();
		ts = bstrcatu(ts, '\'', &error);
		ts = bstrcatu(ts, c, &error);
		ts = bstrcatu(ts, '\'', &error);
		c = (ucp)random() % (UTF8_MAX_POINT + 1);
		assert(parse_char_lit(ts, &c) && c == tc);

		// Reject leading whitespace as in " 'x'"
		const char ws[] = { ' ', '\f', '\n', '\r', '\t', '\v' };
		bfree(ts);
		ts = balloc();
		ts = bstrcatu(ts, ws[random() % sizeof(ws)], &error);
		ts = bstrcatu(ts, '\'', &error);
		ts = bstrcatu(ts, c, &error);
		ts = bstrcatu(ts, '\'', &error);
		c = (ucp)random() % (UTF8_MAX_POINT + 1);
		tc = c;
		assert(!parse_char_lit(ts, &c) && c == tc);

		// Reject trailing whitespace as in "'x' "
		bfree(ts);
		ts = balloc();
		ts = bstrcatu(ts, '\'', &error);
		ts = bstrcatu(ts, c, &error);
		ts = bstrcatu(ts, '\'', &error);
		ts = bstrcatu(ts, ws[random() % sizeof(ws)], &error);
		c = (ucp)random() % (UTF8_MAX_POINT + 1);
		tc = c;
		assert(!parse_char_lit(ts, &c) && c == tc);

		// Reject false null termination of B-string as in "'x'\0"
		bfree(ts);
		ts = balloc();
		ts = bstrcatu(ts, '\'', &error);
		ts = bstrcatu(ts, c, &error);
		ts = bstrcatu(ts, '\'', &error);
		ts = bstrcatu(ts, '\0', &error);
		c = (ucp)random() % (UTF8_MAX_POINT + 1);
		tc = c;
		assert(!parse_char_lit(ts, &c) && c == tc);

		// Test rejection of empty hexadecimal escape '\x'
		bfree(ts);
		ts = balloc();
		ts = bstrcatu(ts, '\'', &error);
		ts = bstrcatu(ts, '\\', &error);
		ts = bstrcatu(ts, 'x', &error);
		ts = bstrcatu(ts, '\'', &error);
		ts = bstrcatu(ts, '\0', &error);
		c = (ucp)random() % (UTF8_MAX_POINT + 1);
		tc = c;
		assert(!parse_char_lit(ts, &c) && c == tc);

		// Test hexadecimal escapes, 1-8 characters in length, e.g. '\x0'
		size_t esclen = (unsigned)random() % 8 + 1;
		char ca[13];
		ca[0] = '\'';
		ca[1] = '\\';
		ca[2] = 'x';
		for (size_t j = 0; j < esclen; j++) {
			unsigned nib = (c >> (4 * (esclen - j - 1))) % 16;
			if (nib >= 10) nib += 'a' - 10;
			else nib += '0';
			ca[3 + j] = nib;
		}
		ca[3 + esclen] = '\'';
		ca[4 + esclen] = '\0';
		bfree(ts);
		ts = bstrdupc(ca);
		tc = c & (((size_t)1 << (esclen * 4)) - 1); // Zero portion not in literal
		c = (ucp)random() % (UTF8_MAX_POINT + 1);
		assert(parse_char_lit(ts, &c) && c == tc);

		// Test octal escapes, 1-3 characters in length, e.g. '\0'
		esclen = (unsigned)random() % 3 + 1;
		ca[0] = '\'';
		ca[1] = '\\';
		for (size_t j = 0; j < esclen; j++) {
			unsigned onib = (c >> (3 * (esclen - j - 1))) % 8 + '0';
			ca[2 + j] = onib;
		}
		ca[2 + esclen] = '\'';
		ca[3 + esclen] = '\0';
		bfree(ts);
		ts = bstrdupc(ca);
		tc = c & (((ucp)1 << (esclen * 3)) - 1); // Zero portion not in literal
		c = (ucp)random() % (UTF8_MAX_POINT + 1);
		assert(parse_char_lit(ts, &c) && c == tc);

		bfree(ts);
	}

	return 0;
}
