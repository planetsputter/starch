// littest.c

#include <assert.h>
#include <ctype.h>
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
	// List in order of increasing c
	{ '\"', '"' },
	{ '\'', '\'' },
	{ '?', '\?' },
	{ '\\', '\\' },
	{ 'a', '\a' },
	{ 'b', '\b' },
	{ 'f', '\f' },
	{ 'n', '\n' },
	{ 'r', '\r' },
	{ 't', '\t' },
	{ 'v', '\v' },
};

int main()
{
	// Get current time
	struct timeval tv;
	int ret = gettimeofday(&tv, NULL);
	if (ret != 0) {
		fprintf(stderr, "error: failed to get current time\n");
		return 1;
	}

	// Seed random number generator with current time in microseconds
	srandom(tv.tv_usec + tv.tv_sec * 1000000);

	ucp c, tc;
	ucp ca[5];
	bchar *ts = NULL;
	int error = 0;

	// Test only valid and no invalid single character escapes for code points below 256
	utf8_decode_array((const byte*)"'\\x'", 4, ca, sizeof(ca) / sizeof(*ca), &error);
	assert(error == 0);
	const struct esc_pair *next_esc = esc_pairs,
		*end_esc = esc_pairs + sizeof(esc_pairs) / sizeof(*esc_pairs);
	for (size_t i = 0; i < 256; i++) {
		ca[2] = i;
		ts = bstrdupu(ca, 4, &error);
		assert(error == 0);
		c = (ucp)random();
		if (next_esc < end_esc && (size_t)next_esc->c == i) { // Valid escape
			assert(parse_char_lit(ts, &c) && c == next_esc->v);
			next_esc++;
		}
		else if (!isdigit((int)i) && i != 'x') { // Invalid escape
			tc = c;
			assert(!parse_char_lit(ts, &c) && c == tc);
		}
		bfree(ts);
	}

	// Test rejection of missing escape
	utf8_decode_array((const byte*)"'\\'", 3, ca, sizeof(ca) / sizeof(*ca), &error);
	assert(error == 0);
	ts = bstrdupu(ca, 3, &error);
	assert(error == 0);
	c = (ucp)random();
	tc = c;
	assert(!parse_char_lit(ts, &c) && c == tc);
	bfree(ts);

	enum { TEST_CYCLES = 1000 };

	for (int i = 0; i < TEST_CYCLES; i++) {
		// Any single-character B-string is an invalid character literal

		// Generate and encode a random UTF-8 character
		c = (ucp)random() % (UTF8_MAX_POINT + 1);

		// Allocate B-string
		ts = bstrdupu(&c, 1, &error);
		assert(error == 0);

		// Assert not valid and output unchanged
		c = (ucp)random() % (UTF8_MAX_POINT + 1);
		tc = c;
		assert(!parse_char_lit(ts, &c) && c == tc);

		// Any two-character B-string is an invalid character literal

		// Append random character to B-string
		ts = bstrcatu(ts, &c, 1, &error);
		assert(error == 0);

		// Assert not valid and output unchanged
		c = (ucp)random() % (UTF8_MAX_POINT + 1);
		tc = c;
		assert(!parse_char_lit(ts, &c) && c == tc);

		// Any B-string with format 'x' where x is not ' or \ is a valid character literal
		utf8_decode_array((const byte*)"'x'", 3, ca, sizeof(ca) / sizeof(*ca), &error);
		assert(error == 0);
		if (c == '\'' || c == '\\') c++;
		ca[1] = c;
		bfree(ts);
		ts = bstrdupu(ca, 3, &error);
		assert(error == 0);
		tc = c;
		c = (ucp)random() % (UTF8_MAX_POINT + 1);
		assert(parse_char_lit(ts, &c) && c == tc);

		// Reject leading whitespace as in " 'x'"
		utf8_decode_array((const byte*)" 'x'", 4, ca, sizeof(ca) / sizeof(*ca), &error);
		assert(error == 0);
		const char ws[] = { ' ', '\f', '\n', '\r', '\t', '\v' };
		ca[0] = ws[random() % sizeof(ws)];
		ca[2] = c;
		bfree(ts);
		ts = bstrdupu(ca, 4, &error);
		assert(error == 0);
		c = (ucp)random() % (UTF8_MAX_POINT + 1);
		tc = c;
		assert(!parse_char_lit(ts, &c) && c == tc);

		// Reject trailing whitespace as in "'x' "
		utf8_decode_array((const byte*)"'x' ", 4, ca, sizeof(ca) / sizeof(*ca), &error);
		assert(error == 0);
		ca[1] = c;
		ca[3] = ws[random() % sizeof(ws)];
		bfree(ts);
		ts = bstrdupu(ca, 4, &error);
		assert(error == 0);
		c = (ucp)random() % (UTF8_MAX_POINT + 1);
		tc = c;
		assert(!parse_char_lit(ts, &c) && c == tc);

		// Reject false null termination of B-string as in "'x'\0"
		utf8_decode_array((const byte*)"'x'\0", 4, ca, sizeof(ca) / sizeof(*ca), &error);
		assert(error == 0);
		ca[1] = c;
		bfree(ts);
		ts = bstrdupu(ca, 4, &error);
		assert(error == 0);
		c = (ucp)random() % (UTF8_MAX_POINT + 1);
		tc = c;
		assert(!parse_char_lit(ts, &c) && c == tc);

		// Test rejection of empty hexadecimal escape '\x'
		bfree(ts);
		ts = bstrdupc("'\\x'");
		c = (ucp)random() % (UTF8_MAX_POINT + 1);
		tc = c;
		assert(!parse_char_lit(ts, &c) && c == tc);

		// Test hexadecimal escapes, 1-8 characters in length, e.g. '\x0'
		size_t esclen = (unsigned)random() % 8 + 1;
		char ba[13];
		ba[0] = '\'';
		ba[1] = '\\';
		ba[2] = 'x';
		for (size_t j = 0; j < esclen; j++) {
			unsigned nib = (c >> (4 * (esclen - j - 1))) % 16;
			if (nib >= 10) nib += 'a' - 10;
			else nib += '0';
			ba[3 + j] = nib;
		}
		ba[3 + esclen] = '\'';
		ba[4 + esclen] = '\0';
		bfree(ts);
		ts = bstrdupc(ba);
		tc = c & (((size_t)1 << (esclen * 4)) - 1); // Zero portion not in literal
		c = (ucp)random() % (UTF8_MAX_POINT + 1);
		assert(parse_char_lit(ts, &c) && c == tc);

		// Test octal escapes, 1-3 characters in length, e.g. '\0'
		esclen = (unsigned)random() % 3 + 1;
		ba[0] = '\'';
		ba[1] = '\\';
		for (size_t j = 0; j < esclen; j++) {
			unsigned onib = (c >> (3 * (esclen - j - 1))) % 8 + '0';
			ba[2 + j] = onib;
		}
		ba[2 + esclen] = '\'';
		ba[3 + esclen] = '\0';
		bfree(ts);
		ts = bstrdupc(ba);
		tc = c & (((ucp)1 << (esclen * 3)) - 1); // Zero portion not in literal
		c = (ucp)random() % (UTF8_MAX_POINT + 1);
		assert(parse_char_lit(ts, &c) && c == tc);

		bfree(ts);
	}

	return 0;
}
