// parser.c

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "bstr.h"
#include "parser.h"
#include "starch.h"

// Returns the nibble value of the given hexzdecimal character
static char nibble_for_hex(char hex)
{
	if (hex >= 'a') return hex - 'a' + 10;
	if (hex >= 'A') return hex - 'A' + 10;
	return hex - '0';
}

// Parse a potentially escaped character literal from the given string, setting *val.
// Returns a pointer to the next character, or NULL if the escape sequence is invalid.
// The goal is for the format of the escape sequences to be identical to those in the C language.
static const char *parse_char_lit(const char *str, char *val)
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

// Parse an entire string of potentially escaped characters, appending unescaped values to the given B-string destination.
// Destination string will have characters appended until end of string or first invalid escape sequence.
// Returns whether the string literal is valid.
static bool parse_string_lit(const char *str, char **bdest)
{
	char cval;
	for (; *str; ) {
		str = parse_char_lit(str, &cval);
		if (str) { // Valid unescape
			*bdest = bstr_append(*bdest, cval);
		}
		else {
			break;
		}
	}
	return str != NULL;
}

// Parse an integer literal. Returns whether the conversion was successful.
static bool parse_int(const char *s, long long *val)
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

// Write the 64-bit unsigned value to eight bytes in little-endian order
static void u64_to_little8(uint64_t val, uint8_t *data)
{
	data[0] = val;
	data[1] = val >> 8;
	data[2] = val >> 16;
	data[3] = val >> 24;
	data[4] = val >> 32;
	data[5] = val >> 40;
	data[6] = val >> 48;
	data[7] = val >> 56;
}

static uint64_t little8_to_u64(const uint8_t *data)
{
	return (uint64_t)data[0] | ((uint64_t)data[1] << 8) |
		((uint64_t)data[2] << 16) | ((uint64_t)data[3] << 24) |
		((uint64_t)data[4] << 32) | ((uint64_t)data[5] << 40) |
		((uint64_t)data[6] << 48) | ((uint64_t)data[7] << 56);
}

void parser_event_init(struct parser_event *pe)
{
	memset(pe, 0, sizeof(*pe));
}

int parser_event_print(const struct parser_event *pe, FILE *outfile)
{
	switch (pe->type) {
	case PET_NONE: break;
	case PET_INST: {
		const char *name = name_for_opcode(pe->inst.opcode);
		fprintf(outfile, "%s", name ? name : "unknown_opcode");
		int immCount = imm_count_for_opcode(pe->inst.opcode);
		if (immCount) { // @todo: Handle more than one immediate value
			fprintf(outfile, " 0x%lx", little8_to_u64(pe->inst.imm));
		}
		fprintf(outfile, "\n");
	}	break;
	case PET_SECTION:
		fprintf(outfile, ".section 0x%lx\n", pe->sec.addr);
		break;
	default:
		fprintf(outfile, "unknown_parser_event\n");
	}
	return ferror(outfile);
}

void parser_init(struct parser *parser)
{
	memset(parser, 0, sizeof(struct parser));
	utf8_decoder_init(&parser->decoder);
	parser->line = 1;
	parser->ch = 1;
	parser_event_init(&parser->event);
	smap_init(&parser->defs, bstr_free);
}

void parser_destroy(struct parser *parser)
{
	if (parser->token) {
		bstr_free(parser->token);
		parser->token = NULL;
	}
	if (parser->defkey) {
		bstr_free(parser->defkey);
		parser->defkey = NULL;
	}
	smap_destroy(&parser->defs);
}

enum { // Parser syntax states
	PSS_DEFAULT,
	PSS_QUOTED,
	PSS_ESCAPED,
	PSS_COMMENT,
};

enum { // Parser token states
	PTS_DEFAULT,
	PTS_IMM,
	PTS_EOL = PTS_IMM + num_dt,
	PTS_DEF_KEY,
	PTS_DEF_VAL,
	PTS_SEC_ADDR,
	PTS_SEC_FLAGS,
};

static int parser_finish_token(struct parser *parser)
{
	if (!parser->token) return 0; // Nothing to finish

	// Perform symbolic substitution on non-quoted tokens
	int ret = 0;
	char *symbol = NULL;
	if (parser->ss != PSS_QUOTED && parser->token[0] == '$') {
		if (parser->token[1] == '\0') { // Check for empty symbol name
			fprintf(stderr, "error: empty symbol name line %d char %d\n",
				parser->tline, parser->tch);
			ret = 1;
		}
		else { // Look up an existing symbol definition
			symbol = smap_get(&parser->defs, parser->token + 1);
			if (!symbol) {
				fprintf(stderr, "error: undefined symbol \"%s\" line %d char %d\n",
					parser->token + 1, parser->tline, parser->tch);
				ret = 1;
			}
		}
	}
	else {
		symbol = parser->token;
	}

	if (parser->ss == PSS_QUOTED) {
		// Currently quoted tokens are not used
		fprintf(stderr, "error: unexpected quoted string line %d char %d\n",
			parser->tline, parser->tch);
		ret = 1;
	}

	if (ret == 0) switch (parser->ts) {
	//
	// Default state
	//
	case PTS_DEFAULT: {
		if (parser->token[0] == '.') { // '.' introduces an assembler command
			if (strcmp(parser->token + 1, "define") == 0) {
				parser->ts = PTS_DEF_KEY;
			}
			else if (strcmp(parser->token + 1, "section") == 0) {
				parser->ts = PTS_SEC_ADDR;
			}
			// @todo: allow to ".include" a file
			else {
				fprintf(stderr, "error: unrecognized assembler command \"%s\" line %d char %d\n",
					parser->token, parser->tline, parser->tch);
				ret = 1;
			}
			break;
		}

		// Look up opcode
		int opcode = opcode_for_name(symbol);
		if (opcode < 0) {
			fprintf(stderr, "error: unrecognized opcode \"%s\" line %d char %d\n",
				symbol, parser->tline, parser->tch);
			ret = 1;
			break;
		}

		// Look up immediate count for opcode
		int imm_count = imm_count_for_opcode(opcode);
		if (imm_count == 1) {
			// Look up immediate type for opcode
			int imm_type;
			int ret = imm_types_for_opcode(opcode, &imm_type);
			if (ret != 0) {
				fprintf(stderr, "error: failed to look up immediate type for opcode %s line %d char %d\n",
					symbol, parser->tline, parser->tch);
				ret = 1;
				break;
			}
			parser->ts = PTS_IMM + imm_type;
		}
		else if (imm_count == 0) {
			// Opcode has no immediates
			parser->ts = PTS_EOL;
		}
		else {
			// Currently there are no instructions with more than one immediate value
			fprintf(stderr, "error: invalid immediate count %d for opcode %s line %d char %d\n",
				imm_count, symbol, parser->tline, parser->tch);
			ret = 1;
			break;
		}

		// Begin the instruction event
		parser->event.type = PET_INST;
		parser->event.inst.opcode = opcode;
	}	break;

	//
	// Parse immediate value
	//
	case PTS_IMM + dt_a8:
	case PTS_IMM + dt_u8:
	case PTS_IMM + dt_i8:
	case PTS_IMM + dt_a16:
	case PTS_IMM + dt_u16:
	case PTS_IMM + dt_i16:
	case PTS_IMM + dt_a32:
	case PTS_IMM + dt_u32:
	case PTS_IMM + dt_i32:
	case PTS_IMM + dt_a64:
	case PTS_IMM + dt_u64:
	case PTS_IMM + dt_i64: {
		// Parse immediate literal
		long long litval;
		bool success = parse_int(symbol, &litval);
		if (!success) {
			fprintf(stderr, "error: failed to parse literal \"%s\" line %d char %d\n",
				symbol, parser->tline, parser->tch);
			ret = 1;
			break;
		}

		// Check bounds
		int dt = parser->ts - PTS_IMM;
		int dt_size = size_for_dt(dt);
		long long minval, maxval;
		min_max_for_dt(dt, &minval, &maxval);
		if (dt_size < 8 && (litval < minval || litval > maxval)) {
			fprintf(stderr, "error: literal \"%s\" is out of bounds for type\n", symbol);
			ret = 1;
			break;
		}

		// Transform literal value to byte array
		parser->event.inst.immlen = dt_size;
		u64_to_little8((uint64_t)litval, parser->event.inst.imm);

		parser->ts = PTS_EOL; // Expect end of line
	}	break;

	//
	// Expect end of line
	//
	case PTS_EOL:
		fprintf(stderr, "error: expected eol at line %d char %d\n", parser->tline, parser->tch);
		ret = 1;
		break;

	//
	// Symbol definition
	//
	case PTS_DEF_KEY: // Symbol definition key
		if (symbol == parser->token) { // No lookup was performed
			parser->defkey = parser->token;
			parser->token = NULL;
		}
		else { // Lookup was performed
			parser->defkey = bstr_dup(symbol);
		}
		parser->ts = PTS_DEF_VAL;
		break;
	case PTS_DEF_VAL: // Symbol definition value
		if (symbol == parser->token) { // No lookup was performed
			parser->token = NULL;
		}
		else { // Lookup was performed
			symbol = bstr_dup(symbol);
		}
		// smap takes ownership of the strings
		smap_insert(&parser->defs, parser->defkey, symbol);
		parser->defkey = NULL;
		parser->ts = PTS_EOL;
		break;

	//
	// Section
	//
	case PTS_SEC_ADDR: {
		// Parse immediate literal
		long long litval;
		bool success = parse_int(symbol, &litval);
		if (!success) {
			fprintf(stderr, "error: failed to parse literal \"%s\" line %d char %d\n",
				symbol, parser->tline, parser->tch);
			ret = 1;
			break;
		}

		// Begin section event
		parser->event.type = PET_SECTION;
		parser->event.sec.addr = litval;

		parser->ts = PTS_EOL; // Expect end of line
	}	break;

	default:
		fprintf(stderr, "error: bad token state\n");
		ret = 1;
		break;
	}

	if (parser->token) {
		bstr_free(parser->token);
		parser->token = NULL;
	}
	return ret;
}

static int parser_finish_line(struct parser *parser, struct parser_event *pe)
{
	if (parser->ts == PTS_EOL) { // Expected EOL
		parser->ts = PTS_DEFAULT;
	}
	if (parser->ts != PTS_DEFAULT) {
		fprintf(stderr, "error: unexpected EOL line %d char %d\n",
			parser->line, parser->ch);
		return 1;
	}
	if (parser->event.type != PET_NONE) {
		// Emit parser event
		memcpy(pe, &parser->event, sizeof(*pe));
		parser_event_init(&parser->event);
	}
	return 0;
}

int parser_parse_byte(struct parser *parser, byte b, struct parser_event *pe)
{
	int ret = 0;

	// Decode the given byte
	ucp c;
	ucp *next = utf8_decoder_decode(&parser->decoder, b, &c, &ret);
	if (ret) {
		fprintf(stderr, "error: UTF-8 decoding error line %d char %d\n",
			parser->line, parser->ch);
		return ret;
	}
	if (next == &c) { // No character generated
		return ret;
	}

	// Re-encode a completed character
	byte enc[5];
	*utf8_encode_char(c, enc, sizeof(enc) - 1, &ret) = '\0';
	if (ret) {
		fprintf(stderr, "error: UTF-8 encoding error line %d char %d\n",
			parser->line, parser->ch);
		return ret;
	}

	// Process the decoded character
	switch (parser->ss) {
	case PSS_DEFAULT:
		if (c == '"') { // Begin quoted token
			ret = parser_finish_token(parser); // First finish any adjacent token
			if (ret) break;
			// Start new token
			parser->token = bstr_alloc();
			parser->tline = parser->line;
			parser->tch = parser->ch;
			parser->ss = PSS_QUOTED;
		}
		else if (isalnum(c) || c == '$' || c == '.' || c == '-' || c == '_' || c == '\'' ||
			c == '\\' || c >= 0x80) { // These continue or start a token
			if (parser->token == NULL) { // Start a new token
				parser->token = bstr_alloc();
				parser->tline = parser->line;
				parser->tch = parser->ch;
			}
			parser->token = bstr_cat(parser->token, (const char*)enc);
		}
		else { // Anything else finishes a token
			ret = parser_finish_token(parser);
			if (ret) break;
			if (c == ';') { // Comments are introduced by ';'
				parser->ss = PSS_COMMENT;
				ret = parser_finish_line(parser, pe);
			}
			else if (c == '\n') { // Line end
				ret = parser_finish_line(parser, pe);
			}
			else if (!isspace(c)) { // Everything else gets its own single-character token
				parser->token = bstr_alloc();
				parser->tline = parser->line;
				parser->tch = parser->ch;
				parser->token = bstr_cat(parser->token, (const char *)enc);
				ret = parser_finish_token(parser);
			}
		}
		break;

	case PSS_QUOTED: // Character in quoted string
		if (c == '"') { // Unescaped '"' ends quoted token
			// Parse any escape sequences in parser->token
			char *unesc_token = bstr_alloc();
			bool result = parse_string_lit(parser->token, &unesc_token);
			if (!result) {
				fprintf(stderr, "error: invalid string literal line %d char %d\n",
					parser->tline, parser->tch);
				ret = 1;
				break;
			}
			bstr_free(parser->token);
			parser->token = unesc_token;
			ret = parser_finish_token(parser); // Finish token
			parser->ss = PSS_DEFAULT;
		}
		else { // All else gets added to token
			parser->token = bstr_cat(parser->token, (const char*)enc);
			if (c == '\\') { // Backslash begins escape sequence
				parser->ss = PSS_ESCAPED;
			}
		}
		break;

	case PSS_ESCAPED: // Escaped character in quoted string
		// Add character to current token
		parser->token = bstr_cat(parser->token, (const char*)enc);
		parser->ss = PSS_QUOTED;
		break;

	case PSS_COMMENT:
		if (c == '\n') { // Comments are ended by newline
			parser->ss = PSS_DEFAULT;
		}
		break;

	default: // Bad parser syntax state
		fprintf(stderr, "error: bad syntax state\n");
		ret = 1;
		break;
	}

	// Keep track of line and character
	if (c == '\n') {
		parser->line++;
		parser->ch = 1;
	}
	else {
		parser->ch++;
	}

	return ret;
}

int parser_can_terminate(struct parser *parser)
{
	return utf8_decoder_can_terminate(&parser->decoder) &&
		parser->token == NULL &&
		(parser->ts == PTS_DEFAULT || parser->ts == PTS_EOL);
}

int parser_terminate(struct parser *parser, struct parser_event *pe)
{
	return parser_finish_line(parser, pe);
}
