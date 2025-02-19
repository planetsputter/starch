// parser.c

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "bstr.h"
#include "parser.h"
#include "starch.h"
#include "stub.h"

// Parse an integer literal. Returns whether the conversion was successful.
static bool parse_int(const char *s, long long *val)
{
	if (*s == '\0') return false; // Empty string

	// Allow character notation such as 'c', '\n'
	long long temp_val = 0;
	if (*s == '\'') {
		s++;
		if (*s == '\0' || *s == '\n' || *s == '\'') {
			// Empty character literal
			return false;
		}
		if (*s == '\\') { // Escaped character
			s++;
			switch (*s) {
			case 'a': temp_val = '\a'; break;
			case 'b': temp_val = '\b'; break;
			case 'f': temp_val = '\f'; break;
			case 'n': temp_val = '\n'; break;
			case 'r': temp_val = '\r'; break;
			case 't': temp_val = '\t'; break;
			case 'v': temp_val = '\v'; break;
			case '\\': temp_val = '\\'; break;
			case '\'': temp_val = '\''; break;
			case '\"': temp_val = '\"'; break;
			case '?': temp_val = '\?'; break;
			default:
				// @todo: allow hexadecimal notation as in '\x00'
				return false;
			}
		}
		else { // Unescaped character
			temp_val = *s;
		}
		s++;
		// Expect end quotation mark
		if (*s != '\'') {
			return false;
		}
		s++;
		// Expect end of string
		if (*s != '\0') {
			return false;
		}
		*val = temp_val;
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

int parser_init(struct parser *parser, FILE *outfile)
{
	memset(parser, 0, sizeof(struct parser));
	utf8_decoder_init(&parser->decoder);
	parser->line = 1;
	parser->ch = 1;
	smap_init(&parser->defs, bstr_free);
	parser->outfile = outfile;
	return stub_init(parser->outfile, 1);
}

void parser_destroy(struct parser *parser)
{
	if (parser->token) {
		bstr_free(parser->token);
		parser->token = NULL;
	}
	smap_destroy(&parser->defs);
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

enum { // Parser syntax states
	PSS_DEFAULT,
	PSS_COMMENT,
};

enum { // Parser token states
	PTS_DEFAULT,
	PTS_IMM,
	PTS_EOL = PTS_IMM + num_dt,
	PTS_DEF_KEY,
	PTS_DEF_VAL,
};

static int parser_finish_token(struct parser *parser)
{
	if (!parser->token) return 0;

	// Look up any existing symbol definition
	// @todo: Require preceding "$" to look up symbols?
	// This would make it more clear when a symbol is being used.
	const char *symbol = smap_get(&parser->defs, parser->token);
	if (!symbol) symbol = parser->token;

	int ret = 0;
	switch (parser->ts) {
	case PTS_DEFAULT: {
		// See if this is a symbol definition
		if (strcmp(parser->token, ".define") == 0) {
			parser->ts = PTS_DEF_KEY;
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
		else if (imm_count != 0) {
			fprintf(stderr, "error: invalid immediate count %d for opcode %s line %d char %d\n",
				imm_count, symbol, parser->tline, parser->tch);
			ret = 1;
			break;
		}

		// Write opcode to file
		uint8_t op = opcode;
		size_t written = fwrite(&op, 1, 1, parser->outfile);
		if (written != 1) {
			fprintf(stderr, "error: failed to write to output file\n");
			ret = 1;
			break;
		}
	}	break;

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

		// Transform to byte array
		uint8_t bytes[8];
		u64_to_little8((uint64_t)litval, bytes);

		// Write to file
		size_t written = fwrite(bytes, 1, dt_size, parser->outfile);
		if (written != (size_t)dt_size) {
			fprintf(stderr, "error: failed to write to output file\n");
			ret = 1;
			break;
		}

		parser->ts = PTS_EOL; // Expect end of line
	}	break;

	case PTS_EOL:
		fprintf(stderr, "error: expected eol at line %d char %d\n", parser->tline, parser->tch);
		ret = 1;
		break;

	case PTS_DEF_KEY: // Symbol definition key
		parser->defkey = parser->token;
		parser->token = NULL;
		parser->ts = PTS_DEF_VAL;
		break;

	case PTS_DEF_VAL: // Symbol definition value
		// smap takes ownership of the strings
		smap_insert(&parser->defs, parser->defkey, parser->token);
		parser->token = NULL;
		parser->ts = PTS_EOL;
		break;

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

static int parser_finish_line(struct parser *parser)
{
	if (parser->ts == PTS_EOL) { // Expected EOL
		parser->ts = PTS_DEFAULT;
	}
	if (parser->ts != PTS_DEFAULT) {
		fprintf(stderr, "error: unexpected EOL line %d char %d\n",
			parser->line, parser->ch);
		return 1;
	}
	return 0;
}

int parser_parse_byte(struct parser *parser, byte b)
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
		if (isalnum(c) || c == '.' || c == '-' || c == '_' || c == '\'' || c == '\\' ||
			c >= 0x80) { // These continue or start a token
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
				ret = parser_finish_line(parser);
			}
			else if (c == '\n') { // Line end
				ret = parser_finish_line(parser);
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

	case PSS_COMMENT:
		if (c == '\n') { // Comments are ended by netline
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

// Assembler commands
enum {
	AC_NONE = 0,
	AC_DEFINE_KEY,
	AC_DEFINE_VAL,
};

int parser_terminate(struct parser *parser)
{
	// Save the first section
	struct stub_sec sec;
	// @todo: make configurable
	sec.addr = 0x1000;
	sec.flags = STUB_FLAG_TEXT;
	return stub_save_section(parser->outfile, 0, &sec);
}
