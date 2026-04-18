// littest.c

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#include "bstr.h"
#include "lits.h"

static int nibtohex(int nib)
{
	if (nib < 10) return '0' + nib;
	return nib + 'a' - 10;
}

// Returns a random Unicode code point, favoring lower values in a logarithmic fashion
static ucp randcp()
{
	long r = random();
	ucp c = (ucp)r % (UTF8_MAX_POINT + 1);
	int bits = ((unsigned long)r / (UTF8_MAX_POINT + 1)) % 21 + 1;
	return c & ((1 << bits) - 1);
};

// Single-character escape codes
struct esc_pair {
	char c;
	ucp v;
};
static struct esc_pair esc_pairs[] = {
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

	int64_t val;
	ucp c, tc;
	ucp ca[10];
	bchar *ts = NULL;
	int error = (int)random();

	// Test rejection of empty string integer literal
	ts = bstrdupc("");
	assert(!parse_int(ts, &val));
	bfree(ts);

	// Test that only decimal digits are valid single-character integer literals
	for (c = 0; c < 256; c++) {
		ts = bstrdupu(&c, 1, &error);
		assert(error == 0);
		if (c >= '0' && c <= '9') {
			assert(parse_int(ts, &val) && val == c - '0');
		}
		else {
			assert(!parse_int(ts, &val));
		}
		bfree(ts);
	}

	// Test signed single-digit integer literals
	ca[0] = '+';
	for (c = '0'; c <= '9'; c++) {
		ca[1] = c;
		ts = bstrdupu(ca, 2, &error);
		assert(error == 0 && parse_int(ts, &val) && val == c - '0');
		bfree(ts);
	}
	ca[0] = '-';
	for (c = '0'; c <= '9'; c++) {
		ca[1] = c;
		ts = bstrdupu(ca, 2, &error);
		assert(error == 0 && parse_int(ts, &val) && val == (int64_t)'0' - c);
		bfree(ts);
	}

	// Test acceptance of maximum decimal literals
	ts = bstrdupc("18446744073709551615");
	assert(parse_int(ts, &val) && val == -1);
	bfree(ts);
	ts = bstrdupc("+18446744073709551615");
	assert(parse_int(ts, &val) && val == -1);
	bfree(ts);
	ts = bstrdupc("-18446744073709551615");
	assert(parse_int(ts, &val) && val == 1);
	bfree(ts);
	ts = bstrdupc("9223372036854775807");
	assert(parse_int(ts, &val) && val == 0x7fffffffffffffff);
	bfree(ts);
	ts = bstrdupc("+9223372036854775807");
	assert(parse_int(ts, &val) && val == 0x7fffffffffffffff);
	bfree(ts);
	ts = bstrdupc("-9223372036854775807");
	assert(parse_int(ts, &val) && val == -0x7fffffffffffffff);
	bfree(ts);
	ts = bstrdupc("9223372036854775808");
	assert(parse_int(ts, &val) && val == (int64_t)0x8000000000000000);
	bfree(ts);
	ts = bstrdupc("+9223372036854775808");
	assert(parse_int(ts, &val) && val == (int64_t)0x8000000000000000);
	bfree(ts);
	ts = bstrdupc("-9223372036854775808");
	assert(parse_int(ts, &val) && val == (int64_t)0x8000000000000000);
	bfree(ts);

	// Test rejection of overflowing decimal literals
	ts = bstrdupc("18446744073709551616");
	assert(!parse_int(ts, &val));
	bfree(ts);
	ts = bstrdupc("+18446744073709551616");
	assert(!parse_int(ts, &val));
	bfree(ts);
	ts = bstrdupc("-18446744073709551616");
	assert(!parse_int(ts, &val));
	bfree(ts);
	ts = bstrdupc("184467440737095516150");
	assert(!parse_int(ts, &val));
	bfree(ts);
	ts = bstrdupc("+184467440737095516150");
	assert(!parse_int(ts, &val));
	bfree(ts);
	ts = bstrdupc("-184467440737095516150");
	assert(!parse_int(ts, &val));
	bfree(ts);
	ts = bstrdupc("57646075230342348790");
	assert(!parse_int(ts, &val));
	bfree(ts);
	ts = bstrdupc("+57646075230342348790");
	assert(!parse_int(ts, &val));
	bfree(ts);
	ts = bstrdupc("-57646075230342348790");
	assert(!parse_int(ts, &val));
	bfree(ts);

	// Test acceptance of maximum octal literals
	ts = bstrdupc("01777777777777777777777");
	assert(parse_int(ts, &val) && val == -1);
	bfree(ts);
	ts = bstrdupc("+01777777777777777777777");
	assert(parse_int(ts, &val) && val == -1);
	bfree(ts);
	ts = bstrdupc("-01777777777777777777777");
	assert(parse_int(ts, &val) && val == 1);
	bfree(ts);
	ts = bstrdupc("0777777777777777777777");
	assert(parse_int(ts, &val) && val == 0x7fffffffffffffff);
	bfree(ts);
	ts = bstrdupc("+0777777777777777777777");
	assert(parse_int(ts, &val) && val == 0x7fffffffffffffff);
	bfree(ts);
	ts = bstrdupc("-0777777777777777777777");
	assert(parse_int(ts, &val) && val == -0x7fffffffffffffff);
	bfree(ts);
	ts = bstrdupc("01000000000000000000000");
	assert(parse_int(ts, &val) && val == (int64_t)0x8000000000000000);
	bfree(ts);
	ts = bstrdupc("+01000000000000000000000");
	assert(parse_int(ts, &val) && val == (int64_t)0x8000000000000000);
	bfree(ts);
	ts = bstrdupc("-01000000000000000000000");
	assert(parse_int(ts, &val) && val == (int64_t)0x8000000000000000);
	bfree(ts);

	// Test rejection of overflowing octal literals
	ts = bstrdupc("02000000000000000000000");
	assert(!parse_int(ts, &val));
	bfree(ts);
	ts = bstrdupc("+02000000000000000000000");
	assert(!parse_int(ts, &val));
	bfree(ts);
	ts = bstrdupc("-02000000000000000000000");
	assert(!parse_int(ts, &val));
	bfree(ts);

	// Test rejection of incomplete hexadecimal literal 0x
	ts = bstrdupc("0x");
	assert(!parse_int(ts, &val));
	bfree(ts);

	// Test acceptance of maximum hexadecimal literals
	ts = bstrdupc("0xffffffffffffffff");
	assert(parse_int(ts, &val) && val == -1);
	bfree(ts);
	ts = bstrdupc("+0xffffffffffffffff");
	assert(parse_int(ts, &val) && val == -1);
	bfree(ts);
	ts = bstrdupc("-0xffffffffffffffff");
	assert(parse_int(ts, &val) && val == 1);
	bfree(ts);
	ts = bstrdupc("0x7fffffffffffffff");
	assert(parse_int(ts, &val) && val == 0x7fffffffffffffff);
	bfree(ts);
	ts = bstrdupc("+0x7fffffffffffffff");
	assert(parse_int(ts, &val) && val == 0x7fffffffffffffff);
	bfree(ts);
	ts = bstrdupc("-0x7fffffffffffffff");
	assert(parse_int(ts, &val) && val == -0x7fffffffffffffff);
	bfree(ts);
	ts = bstrdupc("0x8000000000000000");
	assert(parse_int(ts, &val) && val == (int64_t)0x8000000000000000);
	bfree(ts);
	ts = bstrdupc("+0x8000000000000000");
	assert(parse_int(ts, &val) && val == (int64_t)0x8000000000000000);
	bfree(ts);
	ts = bstrdupc("-0x8000000000000000");
	assert(parse_int(ts, &val) && val == (int64_t)0x8000000000000000);
	bfree(ts);

	// Test rejection of overflowing hexadecimal literals
	ts = bstrdupc("0x10000000000000000");
	assert(!parse_int(ts, &val));
	bfree(ts);
	ts = bstrdupc("+0x10000000000000000");
	assert(!parse_int(ts, &val));
	bfree(ts);
	ts = bstrdupc("-0x10000000000000000");
	assert(!parse_int(ts, &val));
	bfree(ts);

	// Test only valid and no invalid single character escapes for code points below 256
	utf8_decode_array((const byte*)"'\\x'", 4, ca, sizeof(ca) / sizeof(*ca), &error);
	assert(error == 0);
	const struct esc_pair *next_esc = esc_pairs,
		*end_esc = esc_pairs + sizeof(esc_pairs) / sizeof(*esc_pairs);
	for (size_t i = 0; i < 256; i++) {
		ca[2] = i;
		error = (int)random();
		ts = bstrdupu(ca, 4, &error);
		assert(error == 0);
		c = randcp();
		if (next_esc < end_esc && (size_t)next_esc->c == i) { // Valid escape
			assert(parse_char_lit(ts, &c) && c == next_esc->v);
			next_esc++;
		}
		else if (i < '0' && i > '7' && i != 'x' && i != 'u' && i != 'U') { // Invalid escape
			tc = c;
			assert(!parse_char_lit(ts, &c) && c == tc);
		}
		bfree(ts);
	}

	// Test rejection of empty character and string literals
	ts = balloc();
	c = randcp();
	tc = c;
	assert(!parse_char_lit(ts, &c) && c == tc);
	error = (int)random();
	bchar *ds = bstrdupu(&c, 1, &error); // Destination string
	assert(error == 0);
	bchar *ts2 = ds; // Temp string
	assert(!parse_string_lit(ts, &ds) && ds == ts2);
	bfree(ds);
	bfree(ts);

	// Test rejection of missing escape
	error = (int)random();
	utf8_decode_array((const byte*)"'\\'", 3, ca, sizeof(ca) / sizeof(*ca), &error);
	assert(error == 0);
	error = (int)random();
	ts = bstrdupu(ca, 3, &error);
	assert(error == 0);
	c = randcp();
	tc = c;
	assert(!parse_char_lit(ts, &c) && c == tc);
	bfree(ts);

	// """ and "\" are invalid
	error = (int)random();
	utf8_decode_array((const byte*)"\"\"\"", 3, ca, sizeof(ca) / sizeof(*ca), &error);
	assert(error == 0);
	error = (int)random();
	ts = bstrdupu(ca, 3, &error);
	assert(error == 0);
	ds = balloc();
	assert(!parse_string_lit(ts, &ds));
	bfree(ts);
	ca[1] = '\\';
	error = (int)random();
	ts = bstrdupu(ca, 3, &error);
	assert(error == 0);
	assert(!parse_string_lit(ts, &ds));
	bfree(ts);
	// "" followed by null is invalid
	ca[1] = '"';
	ca[2] = '\0';
	error = (int)random();
	ts = bstrdupu(ca, 3, &error);
	assert(error == 0);
	assert(!parse_string_lit(ts, &ds));
	bfree(ts);
	bfree(ds);

	// Test rejection of empty hexadecimal escape '\x'
	ts = bstrdupc("'\\x'");
	c = randcp();
	tc = c;
	assert(!parse_char_lit(ts, &c) && c == tc);
	bfree(ts);

	enum { TEST_CYCLES = 1000 };
	char buff[30];
	for (int i = 0; i < TEST_CYCLES; i++) {
		//
		// Integer literals
		//
		// Test hexadecimal notation
		c = randcp();
		snprintf(buff, sizeof(buff), "0x%x", c);
		ts = bstrdupc(buff);
		assert(parse_int(ts, &val) && val == c);
		bfree(ts);
		snprintf(buff, sizeof(buff), "0x%X", c);
		ts = bstrdupc(buff);
		assert(parse_int(ts, &val) && val == c);
		bfree(ts);
		snprintf(buff, sizeof(buff), "+0x%x", c);
		ts = bstrdupc(buff);
		assert(parse_int(ts, &val) && val == c);
		bfree(ts);
		snprintf(buff, sizeof(buff), "+0x%X", c);
		ts = bstrdupc(buff);
		assert(parse_int(ts, &val) && val == c);
		bfree(ts);
		snprintf(buff, sizeof(buff), "-0x%x", c);
		ts = bstrdupc(buff);
		assert(parse_int(ts, &val) && val == -(int)c);
		bfree(ts);
		snprintf(buff, sizeof(buff), "-0x%X", c);
		ts = bstrdupc(buff);
		assert(parse_int(ts, &val) && val == -(int)c);
		bfree(ts);

		// Test decimal notation
		snprintf(buff, sizeof(buff), "%u", c);
		ts = bstrdupc(buff);
		assert(parse_int(ts, &val) && val == c);
		bfree(ts);
		snprintf(buff, sizeof(buff), "+%u", c);
		ts = bstrdupc(buff);
		assert(parse_int(ts, &val) && val == c);
		bfree(ts);
		snprintf(buff, sizeof(buff), "-%u", c);
		ts = bstrdupc(buff);
		assert(parse_int(ts, &val) && val == -(int)c);
		bfree(ts);

		// Test octal notation
		snprintf(buff, sizeof(buff), "0%o", c);
		ts = bstrdupc(buff);
		assert(parse_int(ts, &val) && val == c);
		bfree(ts);
		snprintf(buff, sizeof(buff), "+0%o", c);
		ts = bstrdupc(buff);
		assert(parse_int(ts, &val) && val == c);
		bfree(ts);
		snprintf(buff, sizeof(buff), "-0%o", c);
		ts = bstrdupc(buff);
		assert(parse_int(ts, &val) && val == -(int)c);
		bfree(ts);

		//
		// Character literals
		//
		// Any single-character B-string is an invalid character literal

		// Generate and encode a random UTF-8 character
		c = randcp();

		// Allocate B-string
		error = (int)random();
		ts = bstrdupu(&c, 1, &error);
		assert(error == 0);

		// Assert not valid and output unchanged
		c = randcp();
		tc = c;
		assert(!parse_char_lit(ts, &c) && c == tc);

		// Any two-character B-string is an invalid character literal

		// Append random character to B-string
		error = (int)random();
		ts = bstrcatu(ts, &c, 1, &error);
		assert(error == 0);

		// Assert not valid and output unchanged
		c = randcp();
		tc = c;
		assert(!parse_char_lit(ts, &c) && c == tc);
		bfree(ts);

		// Any B-string with format 'x' where x is not ' or \ is a valid character literal
		error = (int)random();
		utf8_decode_array((const byte*)"'x'", 3, ca, sizeof(ca) / sizeof(*ca), &error);
		assert(error == 0);
		if (c == '\'' || c == '\\') c++;
		ca[1] = c;
		error = (int)random();
		ts = bstrdupu(ca, 3, &error);
		assert(error == 0);
		tc = c;
		c = randcp();
		assert(parse_char_lit(ts, &c) && c == tc);
		bfree(ts);

		// Reject leading whitespace as in " 'x'"
		error = (int)random();
		utf8_decode_array((const byte*)" 'x'", 4, ca, sizeof(ca) / sizeof(*ca), &error);
		assert(error == 0);
		const char ws[] = { ' ', '\f', '\n', '\r', '\t', '\v' };
		ca[0] = ws[random() % sizeof(ws)];
		ca[2] = c;
		error = (int)random();
		ts = bstrdupu(ca, 4, &error);
		assert(error == 0);
		c = randcp();
		tc = c;
		assert(!parse_char_lit(ts, &c) && c == tc);
		bfree(ts);

		// Reject trailing whitespace as in "'x' "
		error = (int)random();
		utf8_decode_array((const byte*)"'x' ", 4, ca, sizeof(ca) / sizeof(*ca), &error);
		assert(error == 0);
		ca[1] = c;
		ca[3] = ws[random() % sizeof(ws)];
		error = (int)random();
		ts = bstrdupu(ca, 4, &error);
		assert(error == 0);
		c = randcp();
		tc = c;
		assert(!parse_char_lit(ts, &c) && c == tc);
		bfree(ts);

		// Reject false null termination of B-string as in "'x'\0"
		error = (int)random();
		utf8_decode_array((const byte*)"'x'\0", 4, ca, sizeof(ca) / sizeof(*ca), &error);
		assert(error == 0);
		ca[1] = c;
		error = (int)random();
		ts = bstrdupu(ca, 4, &error);
		assert(error == 0);
		c = randcp();
		tc = c;
		assert(!parse_char_lit(ts, &c) && c == tc);
		bfree(ts);

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
		ts = bstrdupc(ba);
		tc = c & (((size_t)1 << (esclen * 4)) - 1); // Zero portion not in literal
		c = randcp();
		assert(parse_char_lit(ts, &c) && c == tc);
		if (esclen == 4) {
			// Should get same result with 4-digit Unicode escape
			bfree(ts);
			ba[2] = 'u';
			ts = bstrdupc(ba);
			c = randcp();
			assert(parse_char_lit(ts, &c) && c == tc);
		}
		else if (esclen == 8) {
			// Should get same result with 8-digit Unicode escape
			bfree(ts);
			ba[2] = 'U';
			ts = bstrdupc(ba);
			c = randcp();
			assert(parse_char_lit(ts, &c) && c == tc);
		}
		bfree(ts);

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
		ts = bstrdupc(ba);
		tc = c & (((ucp)1 << (esclen * 3)) - 1); // Zero portion not in literal
		c = randcp();
		assert(parse_char_lit(ts, &c) && c == tc);
		bfree(ts);

		//
		// String literals
		//

		// Any single character string literal is invalid
		c = randcp();
		error = (int)random();
		ts = bstrdupu(&c, 1, &error);
		assert(error == 0);
		c = randcp();
		error = (int)random();
		ds = bstrdupu(&c, 1, &error);
		assert(error == 0);
		ts2 = bstrdupb(ds);
		assert(!parse_string_lit(ts, &ds) && bstrcmpb(ds, ts2) == 0);
		bfree(ts2);

		// Any two character string literal is invalid except ""
		c = randcp();
		if (c == '"' && ts[0] == '"') c++;
		error = (int)random();
		ts = bstrcatu(ts, &c, 1, &error);
		assert(error == 0);
		c = randcp();
		error = (int)random();
		ds = bstrcatu(ds, &c, 1, &error);
		assert(error == 0);
		assert(!parse_string_lit(ts, &ds));
		bfree(ts);
		bfree(ds);

		// Any string literal of the form "x" where x is not " or \ is valid
		error = (int)random();
		utf8_decode_array((const byte*)"\"x\"", 3, ca, sizeof(ca) / sizeof(*ca), &error);
		assert(error == 0);
		c = randcp();
		if (c == '"' || c == '\\') c++;
		ca[1] = c;
		error = (int)random();
		ts = bstrdupu(ca, 3, &error);
		assert(error == 0);
		ds = balloc();
		assert(parse_string_lit(ts, &ds));
		error = (int)random();
		ucp *cp = utf8_decode_array((const byte*)ds, bstrlen(ds), ca, sizeof(ca) / sizeof(*ca), &error);
		assert(error == 0 && cp == ca + 1 && ca[0] == c);
		bfree(ds);
		bfree(ts);

		// "\xhhhhhh" is valid only if the hexadecimal value is less than 256
		error = (int)random();
		utf8_decode_array((const byte*)"\"\\xhhhhhh\"", 10, ca, sizeof(ca) / sizeof(*ca), &error);
		assert(error == 0);
		c = randcp();
		tc = c;
		for (int j = 8; j > 2; j--) {
			ca[j] = nibtohex(c & 0xf);
			c >>= 4;
		}
		error = (int)random();
		ts = bstrdupu(ca, 10, &error);
		assert(error == 0);
		ds = balloc();
		assert(parse_string_lit(ts, &ds) == (tc < 256));
		bfree(ds);
		bfree(ts);
	}

	return 0;
}
