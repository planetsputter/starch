// lits.c
//
// String and integer literal parsing utility functions

#include <ctype.h>
#include <stddef.h>

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

bool parse_string_lit(const bchar *str, bchar **dest)
{
	// Literal must start with '"'
	size_t len = bstrlen(str);
	if (len < 2 || *str != '"') return false;
	const char *end = str + len;

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
				*dest = bstr_append(*dest, cval);
			}
			else {
				int error;
				*dest = bstrcatu(*dest, &cval, 1, &error);
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

bool parse_int(const bchar *s, int64_t *val)
{
	size_t slen = bstrlen(s);

	// Allow character notation such as 'c', '\n'
	ucp c;
	bool ret = parse_char_lit(s, &c);
	if (ret) {
		*val = c;
		return ret;
	}

	const char *p = s;

	// Parse optional sign
	bool neg;
	if (*p == '-') {
		p++;
		neg = true;
	}
	else {
		neg = false;
	}
	if (*p == '\0') return false; // "" or "-"

	// @todo: handle overflow
	int64_t temp_val = 0;

	if (*p == '0') {
		if (*(p + 1) == 'x') { // Hexadecimal literal
			p += 2;
			if (*p == '\0') return false; // "0x"
			for (; isxdigit(*p); p++) {
				// Compute value of hexadecimal digit
				int val = *p >= 'a' ? *p - 'a' + 10 : *p >= 'A' ? *p - 'A' + 10 : *p - '0';
				temp_val = temp_val * 0x10 + val;
			}
		}
		else { // Octal literal
			for (p++; *p >= '0' && *p <= '7'; p++) {
				temp_val = temp_val * 8 + *p - '0';
			}
		}
	}
	else { // Decimal literal
		for (; isdigit(*p); p++) {
			temp_val = temp_val * 10 + *p - '0';
		}
	}

	// Expect end of string
	if ((size_t)(p - s) != slen) return false;

	*val = neg ? -temp_val : temp_val;
	return true;
}
