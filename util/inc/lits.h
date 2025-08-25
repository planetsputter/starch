// lits.h
//
// String and integer literal parsing utility functions

#pragma once

#include <stdbool.h>
#include <stdint.h>

// Parse a potentially escaped character literal from the given string, setting *val.
// Returns a pointer to the next character, or NULL if the escape sequence is invalid.
// The goal is for the format of the escape sequences to be identical to those in the C language.
const char *parse_char_lit(const char *str, char *val);

// Parse an entire string literal, including beginning and ending quotation marks,
// interpreting escapes and appending unescaped content to the given B-string destination.
// Destination string will have characters appended until end of string or first invalid escape sequence.
// Returns whether the string literal is valid.
bool parse_string_lit(const char *str, char **bdest);

// Parse an integer literal, setting *val. Returns whether the conversion was successful.
// Character notation such as 'c' is allowed. Hexadecimal notation beginning with 0x is allowed.
// Decimal literals are also allowed.
bool parse_int(const char *s, int64_t *val);
