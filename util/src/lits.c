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

const char *parse_char_lit(const char *str, char *val)
{
	char tc = 0; // Temporary character
	if (*str == '\\') {
		switch (*++str) {
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
		str++;
	}
	else if (*str == '\0' || *str == '\n') { // Disallowed characters
		return NULL;
	}
	else {
		tc = *str++;
	}
	*val = tc;
	return str;
}

bool parse_string_lit(const char *str, bchar **dest)
{
	// Literal must start with '"'
	if (*str++ != '"') return false;

	char cval;
	for (; *str && *str != '"'; ) {
		str = parse_char_lit(str, &cval);
		if (str) { // Valid unescape
			*dest = bstr_append(*dest, cval);
		}
		else {
			break;
		}
	}

	// Ensure escapes were valid and literal ends at the first unescaped '"'
	return str != NULL && *str == '"' && *(str + 1) == '\0';
}

bool parse_int(const char *s, int64_t *val)
{
	if (*s == '\0') return false; // Empty string

	// Allow character notation such as 'c', '\n'
	long long temp_val = 0;
	if (*s == '\'') {
		s++;
		char cval;
		const char *end = parse_char_lit(s, &cval);
		if (end == NULL || *end != '\'' || *(end + 1) != '\0') {
			return false;
		}
		*val = cval;
		return true;
	}

	bool neg;
	if (*s == '-') {
		s++;
		if (*s == '\0') return false; // "-"
		neg = true;
	}
	else {
		neg = false;
	}

	// @todo: handle overflow
	if (*s == '0' && *(s + 1) == 'x') { // Hexadecimal literal
		s += 2;
		if (*s == '\0') return false; // "0x"
		for (; isxdigit(*s); s++) {
			// Compute value of hexadecimal digit
			int val = *s >= 'a' ? *s - 'a' + 10 : *s >= 'A' ? *s - 'A' + 10 : *s - '0';
			temp_val = temp_val * 0x10 + val;
		}
	}
	else { // Decimal literal
		for (; isdigit(*s); s++) {
			temp_val = temp_val * 10 + *s - '0';
		}
	}

	// Expect end of string
	if (*s != '\0') return false;

	*val = neg ? -temp_val : temp_val;
	return true;
}

