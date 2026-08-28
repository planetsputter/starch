// lits.h
//
// String and integer literal parsing utility functions

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "bstr.h"
#include "utf8.h"

// Parse an entire B-string character literal, including beginning and ending single quotation marks,
// interpreting escapes and setting *val.
// Returns whether the character literal is valid.
bool parse_char_lit(const bchar *str, ucp *val);

// Escapes the given character, appending a character literal to the given destination B-string
void escape_char_lit(ucp val, bchar **dest);

// Parse an entire B-string string literal, including beginning and ending double quotation marks,
// interpreting escapes and appending unescaped content to the given B-string destination.
// Destination string will have characters appended until end of string or first invalid escape sequence.
// If dest is NULL, only a validity check is performed.
// Returns whether the string literal is valid.
bool parse_string_lit(const bchar *str, bchar **dest);

// Escapes the given B-string, appending a string literal to the given destination B-string
void escape_string_lit(const bchar *str, bchar **dest);

// Parse a B-string integer literal, setting *val. Returns whether the conversion was successful.
// Character notation such as 'c' is allowed. Hexadecimal notation beginning with 0x is allowed.
// Decimal literals are also allowed.
// If val is NULL, only a validity check is performed.
// Returns whether the integer literal is valid.
bool parse_int(const bchar *s, int64_t *val);
