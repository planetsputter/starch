// lits.c
//
// String and integer literal parsing utility functions

#include <assert.h>
#include <ctype.h>
#include <stddef.h>
#include <stdio.h>

#include "lits.h"

static char nibble_for_hex(char hex)
{
	if (hex >= 'a') return hex - 'a' + 10;
	if (hex >= 'A') return hex - 'A' + 10;
	return hex - '0';
}

// Parses any escape code at the given string using up to remain bytes.
// On success sets *val to the escaped character value and returns pointer to next unused byte.
// Otherwise returns NULL.
static const char *parse_char_lit_impl(const char *str, size_t remain, ucp *val, int *esctype)
{
	ucp tc = 0; // Temporary character
	if (*str == '\\') {
		char e = *++str;
		switch (e) {
		case 'a': tc = '\a'; break;
		case 'b': tc = '\b'; break;
		case 'f': tc = '\f'; break;
		case 'n': tc = '\n'; break;
		case 'r': tc = '\r'; break;
		case 't': tc = '\t'; break;
		case 'v': tc = '\v'; break;
		case '\\': tc = '\\'; break;
		case '\'': tc = '\''; break;
		case '\"': tc = '\"'; break;
		case '?': tc = '\?'; break;
		case 'x': // Allow hexadecimal notation as in '\x00'
			if (!isxdigit(*++str)) return NULL;
			do { // Hexadecimal escape sequences are of arbitrary length
				tc = (tc << 4) | nibble_for_hex(*str);
			} while (isxdigit(*++str));
			str--;
			break;
		case 'u': // Four-digit Unicode character notation
			str++;
			for (int i = 0; i < 4; i++) {
				if (!isxdigit(*str)) return NULL;
				tc = (tc << 4) | nibble_for_hex(*str);
				str++;
			}
			str--;
			break;
		case 'U': // Eight-digit Unicode character notation
			str++;
			for (int i = 0; i < 8; i++) {
				if (!isxdigit(*str)) return NULL;
				tc = (tc << 4) | nibble_for_hex(*str);
				str++;
			}
			str--;
			break;
		default:
			if (*str < '0' || *str > '7') return NULL; // Invalid escape sequence
			// Octal escape sequences have a maximum of three characters
			tc = *str - '0';
			str++;
			if (*str < '0' || *str > '7') {
				str--;
				break;
			}
			tc = (tc << 3) + *str - '0';
			str++;
			if (*str < '0' || *str > '7') {
				str--;
				break;
			}
			tc = (tc << 3) + *str - '0';
		}
		if (esctype) *esctype = e;
		str++;
	}
	else {
		int error = 0;
		utf8_decode_array((const byte*)str, remain, &tc, 1, &error);
		if (error != UTF8_ERROR_CHARACTER_OVERFLOW && error != 0) return NULL;
		str += utf8_bytes_for_char(tc, &error);
		if (esctype) *esctype = 0;
	}
	*val = tc;
	return str;
}

bool parse_char_lit(const bchar *s, ucp *val)
{
	// Ensure beginning and ending single quotes, valid or no escape, and correct B-string length
	if (*s != '\'') return false;

	size_t slen = bstrlen(s);
	ucp tv = 0;
	const char *end = parse_char_lit_impl(s + 1, slen - 1, &tv, NULL);
	if (end == NULL || *end != '\'' || slen != (size_t)(end - s) + 1) return false;

	*val = tv;
	return true;
}

// Appends the given character or an escape sequence for the given character to the destination B-string.
// If alldigits is true, use three octal digits if octal notation is used. This will prevent the escape
// sequence from running into a following literal octal digit.
static void escape_char_lit_impl(ucp val, bchar **dest, bool alldigits)
{
	if (val < 128) {
		ucp ec = 0;
		switch (val) {
		case '\a': ec = 'a'; break;
		case '\b': ec = 'b'; break;
		case '\f': ec = 'f'; break;
		case '\n': ec = 'n'; break;
		case '\r': ec = 'r'; break;
		case '\t': ec = 't'; break;
		case '\v': ec = 'v'; break;
		case '\\': ec = '\\'; break;
		case '\'': ec = '\''; break;
		case '\"': ec = '\"'; break;
		case '\?': ec = '?'; break;
		default: break;
		}
		char buf[5];
		if (ec) { // There is an escape character for this value
			sprintf(buf, "\\%c", ec);
		}
		else if (val < ' ' || val == 127) { // There is no escape character. Represent in octal.
			sprintf(buf, alldigits ? "\\%03o" : "\\%o", val);
		}
		else { // Represent verbatim
			sprintf(buf, "%c", val);
		}
		*dest = bstrcatc(*dest, buf);
	}
	else { // Append the character verbatim
		int error;
		*dest = bstrcatu(*dest, &val, 1, &error);
		assert(error == 0);
	}
}

void escape_char_lit(ucp val, bchar **dest)
{
	*dest = bstrcatc(*dest, "'");
	escape_char_lit_impl(val, dest, false);
	*dest = bstrcatc(*dest, "'");
}

bool parse_string_lit(const bchar *str, bchar **dest)
{
	// Literal must start with '"'
	size_t len = bstrlen(str);
	if (len < 2 || *str != '"') return false;
	const bchar *end = str + len;

	ucp cval;
	int esctype;
	for (str++; str < end;) {
		if (*str == '"') { // Unescaped '"' ends literal
			break;
		}
		// Parse potentially escaped value
		str = parse_char_lit_impl(str, end - str, &cval, &esctype);
		if (str) {
			if (esctype != 0 && esctype != 'u' && esctype != 'U') {
				// This escape type represents a single char
				if (cval > 255) { // Escaped value too large
					str = NULL;
					break;
				}
				if (dest) *dest = bstr_append(*dest, cval);
			}
			else {
				int error = 0;
				if (dest) *dest = bstrcatu(*dest, &cval, 1, &error);
				if (error) {
					// Escaped Unicode character too large for UTF-8 representation
					str = NULL;
					break;
				}
			}
		}
		else { // Invalid escape sequence
			break;
		}
	}

	// Ensure escapes were valid and literal ends at the first unescaped '"'
	return str != NULL && str == end - 1 && *str == '"';
}

void escape_string_lit(const bchar *str, bchar **dest)
{
	size_t len = bstrlen(str);
	const bchar *end = str + len;

	*dest = bstrcatc(*dest, "\""); // Beginning quotation

	for (; str < end; str++) {
		// We rely on the fact that in UTF-8 encoded strings only bytes with values
		// less than 128 will need to be escaped
		ucp val = (ucp)*str;
		if (val < 128) {
			bchar next = *(str + 1);
			escape_char_lit_impl(val, dest, next >= '0' && next <= '7');
		}
		else {
			*dest = bstr_append(*dest, val);
		}
	}

	*dest = bstrcatc(*dest, "\""); // Ending quotation
}

bool parse_int(const bchar *s, int64_t *val)
{
	size_t slen = bstrlen(s);

	// Allow character notation such as 'c', '\n'
	ucp c;
	bool ret = parse_char_lit(s, &c);
	if (ret) {
		if (val) *val = c;
		return ret;
	}

	const char *p = s;

	// Parse optional sign
	bool neg;
	if (*p == '-') {
		neg = true;
		p++;
	}
	else {
		neg = false;
		if (*p == '+') {
			p++;
		}
	}
	if (*p == '\0') return false; // "", "-", or "+"

	int64_t temp_val = 0;

	if (*p == '0') {
		if (*(p + 1) == 'x') { // Hexadecimal literal
			p += 2;
			if (*p == '\0') return false; // "0x"
			for (; isxdigit(*p); p++) {
				// Compute value of hexadecimal digit
				int nval = *p >= 'a' ? *p - 'a' + 10 : *p >= 'A' ? *p - 'A' + 10 : *p - '0';
				if (temp_val >> 60) return false; // *0x10 would overflow
				temp_val = temp_val * 0x10 + nval;
			}
		}
		else { // Octal literal
			for (p++; *p >= '0' && *p <= '7'; p++) {
				if (temp_val >> 61) return false; // *010 would overflow
				temp_val = temp_val * 010 + *p - '0';
			}
		}
	}
	else { // Decimal literal
		for (; isdigit(*p); p++) {
			// Wrapping (setting high bit and so becoming negative) is okay. Overflow is not.
			if (temp_val > 0x1999999999999999l || temp_val < 0) return false; // *10 would overflow
			// Note: A bug in gcc 15.2.0 requires us to compare temp_val to the greatest value
			// that will not wrap when multiplied by 10, rather than comparing for negative
			// after multiplying by 10. Otherwise when compiled with optimization the check for
			// negative will apparently be skipped.
			bool wrap = temp_val > 0x0cccccccccccccccl; // Values greater than this will wrap
			temp_val = temp_val * 10 + *p - '0';
			if (wrap && (temp_val >= 0)) return false; // Sign flip indicates overflow
		}
	}

	// Expect end of string
	if ((size_t)(p - s) != slen) return false;

	if (val) *val = neg ? -temp_val : temp_val;
	return true;
}
