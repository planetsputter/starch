// parser.c

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "bstr.h"
#include "lits.h"
#include "parser.h"
#include "starch.h"
#include "util.h"

// Automatic symbols, besides instruction opcodes and interrupt numbers
struct autosym {
	const char *name;
	uint64_t val;
};
static struct autosym autosyms[] = {
	// IO addresses
	{ "BEGIN_IO_ADDR", BEGIN_IO_ADDR },
	{ "IO_STDIN_ADDR", IO_STDIN_ADDR },
	{ "IO_STDOUT_ADDR", IO_STDOUT_ADDR },
	{ "IO_FLUSH_ADDR", IO_FLUSH_ADDR },
	{ "IO_URAND_ADDR", IO_URAND_ADDR },
	{ "IO_ASSERT_ADDR", IO_ASSERT_ADDR },
	// Other addresses
	{ "BEGIN_INT_ADDR", BEGIN_INT_ADDR },
	{ "INIT_PC_VAL", INIT_PC_VAL },
};
static struct autosym *get_autosym(const char *name)
{
	struct autosym *ret = NULL;
	for (size_t i = 0; i < sizeof(autosyms) / sizeof(struct autosym); i++) {
		if (strcmp(autosyms[i].name, name) == 0) {
			ret = autosyms + i;
			break;
		}
	}
	return ret;
}

// Returns the minimum number of bytes required to represent the given value
static int min_bytes_for_val(int64_t val)
{
	if (val < (int32_t)0x80000000) return 8;
	if (val < (int16_t)0x8000) return 4;
	if (val < (int8_t)0x80) return 2;
	if (val <= (uint8_t)0xff) return 1;
	if (val <= (uint16_t)0xffff) return 2;
	if (val <= (uint32_t)0xffffffff) return 4;
	return 8;
}

void parser_event_init(struct parser_event *pe)
{
	memset(pe, 0, sizeof(*pe));
}

void parser_event_destroy(struct parser_event *pe)
{
	switch (pe->type) {
	case PET_INST:
		if (pe->inst.imm) {
			bfree(pe->inst.imm);
			pe->inst.imm = NULL;
		}
		break;

	case PET_INCLUDE:
		bfree(pe->filename);
		pe->filename = NULL;
		break;
	
	case PET_LABEL:
		bfree(pe->label);
		pe->label = NULL;
		break;

	default:
		break;
	}
}

int parser_event_print(const struct parser_event *pe, FILE *outfile)
{
	switch (pe->type) {
	case PET_NONE: break;
	case PET_INST: {
		const char *name = name_for_opcode(pe->inst.opcode);
		fprintf(outfile, "%s", name ? name : "unknown_opcode");
		if (pe->inst.imm) {
			fprintf(outfile, " %s\n", pe->inst.imm);
		}
		else {
			fprintf(outfile, "\n");
		}
	}	break;
	case PET_DATA:
		fprintf(outfile, ".data%d %#"PRIx64"\n", pe->data.len, get_little64(pe->data.raw));
		break;
	case PET_SECTION:
		fprintf(outfile, ".section %#"PRIx64"\n", pe->sec.addr);
		break;
	case PET_INCLUDE:
		fprintf(outfile, ".include \"%s\"\n", pe->filename);
		break;
	case PET_LABEL:
		fprintf(outfile, "%s\n", pe->label);
		break;
	case PET_STRINGS:
		fprintf(outfile, ".strings\n");
		break;
	default:
		fprintf(outfile, "unknown_parser_event\n");
	}
	return ferror(outfile);
}

void parser_init(struct parser *parser, bchar *filename)
{
	memset(parser, 0, sizeof(struct parser));
	if (filename) {
		parser->filename = filename;
	}
	else {
		parser->filename = bstrdupc("stdin");
	}
	utf8_decoder_init(&parser->decoder);
	parser->line = 1;
	parser->ch = 1;
	parser_event_init(&parser->event);
	smap_init(&parser->defs, bfree);
}

void parser_destroy(struct parser *parser)
{
	if (parser->filename) {
		bfree(parser->filename);
		parser->filename = NULL;
	}
	if (parser->token) {
		bfree(parser->token);
		parser->token = NULL;
	}
	if (parser->defkey) {
		bfree(parser->defkey);
		parser->defkey = NULL;
	}
	smap_destroy(&parser->defs);
}

enum { // Parser token states
	PTS_DEFAULT,
	PTS_IMM, // Immediate for instruction or raw data
	PTS_PIMM, // Immediate for pseudo-op
	PTS_EOL,
	PTS_DEF_KEY,
	PTS_DEF_VAL,
	PTS_SEC_ADDR,
	PTS_INCLUDE,
};

// Finishes any in-progress token, updating parser->event.
// Returns zero on success.
static int parser_finish_token(struct parser *parser)
{
	if (!parser->token) return 0; // Nothing to finish

	// Perform symbolic substitution
	int ret = 0;
	bchar *symbol = NULL;
	if (parser->token[0] == '$') {
		if (parser->token[1] == '\0') { // Check for empty symbol name
			fprintf(stderr, "error: empty symbol name in \"%s\" line %d char %d\n",
				parser->filename, parser->tline, parser->tch);
			ret = 1;
		}
		else { // Look up an existing symbol definition
			symbol = smap_get(&parser->defs, parser->token + 1);

			if (!symbol) {
				// Check for automatic opcode symbols
				enum { MAX_OPCODE_NAME_LEN = 32 };
				char symbuf[MAX_OPCODE_NAME_LEN + 1];
				if (strncmp(parser->token + 1, "OP_", 3) == 0) {
					// Verify all letters are uppercase and length is below limit
					char *s;
					for (s = parser->token + 4; isupper(*s) || isdigit(*s); s++);
					if (*s == '\0' && (s - parser->token - 4) <= MAX_OPCODE_NAME_LEN) {
						// All letters are uppercase. Convert to lowercase.
						strncpy(symbuf, parser->token + 4, MAX_OPCODE_NAME_LEN);
						symbuf[MAX_OPCODE_NAME_LEN] = '\0';
						for (s = symbuf; *s != '\0'; s++) {
							*s = tolower(*s);
						}
						// Attempt to look up opcode
						int opcode = opcode_for_name(symbuf);
						if (opcode >= 0) {
							sprintf(symbuf, "%d", opcode);
							symbol = bstrdupc(symbuf);
						}
					}
				}
				else {
					// Check for named interrupt numbers
					int stint = stint_for_name(parser->token + 1);
					if (stint >= 0) { // Interrupt name
						sprintf(symbuf, "%d", stint);
						symbol = bstrdupc(symbuf);
					}
					else {
						// Look up other automatic symbols
						struct autosym *as = get_autosym(parser->token + 1);
						if (as) {
							sprintf(symbuf, "%#"PRIx64, as->val);
							symbol = bstrdupc(symbuf);
						}
					}
				}
				if (symbol) { // Found a matching autosymbol
					smap_insert(&parser->defs, bstrdupc(parser->token + 1), symbol);
				}
				else {
					fprintf(stderr, "error: undefined symbol \"%s\" in \"%s\" line %d char %d\n",
						parser->token + 1, parser->filename, parser->tline, parser->tch);
					ret = 1;
				}
			}
		}
	}
	else {
		symbol = parser->token;
	}

	if (ret == 0) switch (parser->ts) {
	//
	// Default state
	//
	case PTS_DEFAULT: {
		if (parser->token[0] == '.') { // '.' introduces an assembler command
			// Symbolic substitution cannot be used for assembler commands
			if (strcmp(parser->token + 1, "define") == 0) {
				parser->ts = PTS_DEF_KEY;
			}
			else if (strcmp(parser->token + 1, "section") == 0) {
				parser->ts = PTS_SEC_ADDR;
			}
			else if (strcmp(parser->token + 1, "include") == 0) {
				parser->ts = PTS_INCLUDE;
			}
			else if (strcmp(parser->token + 1, "data8") == 0) {
				parser->event.type = PET_DATA;
				parser->event.data.len = 1;
				parser->ts = PTS_IMM;
			}
			else if (strcmp(parser->token + 1, "data16") == 0) {
				parser->event.type = PET_DATA;
				parser->event.data.len = 2;
				parser->ts = PTS_IMM;
			}
			else if (strcmp(parser->token + 1, "data32") == 0) {
				parser->event.type = PET_DATA;
				parser->event.data.len = 4;
				parser->ts = PTS_IMM;
			}
			else if (strcmp(parser->token + 1, "data64") == 0) {
				parser->event.type = PET_DATA;
				parser->event.data.len = 8;
				parser->ts = PTS_IMM;
			}
			else if (strcmp(parser->token + 1, "strings") == 0) {
				parser->event.type = PET_STRINGS;
				parser->ts = PTS_EOL;
			}
			else {
				fprintf(stderr, "error: unrecognized assembler command \"%s\" in \"%s\" line %d char %d\n",
					parser->token, parser->filename, parser->tline, parser->tch);
				ret = 1;
			}
		}
		else if (symbol[0] == ':') { // ':' introduces a label
			if (symbol[1] == '\0') {
				fprintf(stderr, "error: empty label in \"%s\" line %d char %d\n",
					parser->filename, parser->tline, parser->tch);
				ret = 1;
			}
			else {
				// Emit label event
				parser->event.type = PET_LABEL;
				if (symbol == parser->token) { // No lookup was performed
					parser->event.label = parser->token;
					parser->token = NULL;
				}
				else { // Lookup was performed
					parser->event.label = bstrdupb(symbol);
				}
			}
			parser->ts = PTS_EOL; // Expect end of line
		}
		else if (symbol[0] == '"') { // Introduces a quoted string
			// Quoted strings are not valid for the first word in a statement
			fprintf(stderr, "error: unexpected quoted string in \"%s\" line %d char %d\n",
				parser->filename, parser->tline, parser->tch);
			ret = 1;
		}
		else { // Everything else must be an instruction
			int opcode = opcode_for_name(symbol);
			if (opcode < 0) {
				// Instruction is not an exact match for any opcode. Check for pseudo-ops.
				// @todo: Add pseudo-ops to a table.
				if (strcmp(symbol, "push8") == 0) {
					opcode = op_push8as8;
				}
				else if (strcmp(symbol, "push16") == 0) {
					opcode = op_push16as16;
				}
				else if (strcmp(symbol, "push32") == 0) {
					opcode = op_push32as32;
				}
				else if (strcmp(symbol, "push64") == 0) {
					opcode = op_push64as64;
				}
				else {
					fprintf(stderr, "error: unrecognized opcode \"%s\" in \"%s\" line %d char %d\n",
						symbol, parser->filename, parser->tline, parser->tch);
					ret = 1;
					break;
				}
				// Note that immediate value to follow is for a pseudo-op
				parser->ts = PTS_PIMM;
			}
			else {
				// Look up immediate type for opcode
				int sdt = imm_type_for_opcode(opcode);
				if (sdt < 0) {
					fprintf(stderr, "error: failed to look up immediate type for opcode %s in \"%s\" line %d char %d\n",
						symbol, parser->filename, parser->tline, parser->tch);
					ret = 1;
					break;
				}
				if (sdt == SDT_VOID) {
					// Opcode has no immediates
					parser->ts = PTS_EOL;
				}
				else {
					// Regular immediate follows
					parser->ts = PTS_IMM;
				}
			}
			// Begin the instruction event
			parser->event.inst.opcode = opcode;
			parser->event.type = PET_INST;
		}

	}	break;

	//
	// Parse immediate value
	//
	case PTS_IMM:
	case PTS_PIMM: {
		// Parser event type will be either PET_INST or PET_DATA.
		// Token state will be PTS_PIMM if opcode was a pseudo-op.
		// Token state will not be PTS_PIMM if event type is PET_DATA.

		if (symbol[0] == '"') { // Immediate is string literal
			if (parser->event.type != PET_INST) {
				// @todo: Possibly support string literal immediates for data64 event type,
				// though this could be confusing because data would be pointer to string elsewhere
				fprintf(stderr, "error: unsupported statement type for string literal in \"%s\" line %d char %d\n",
					parser->filename, parser->tline, parser->tch);
				ret = 1;
				break;
			}

			// String literals can only be used for opcodes that accept 64-bit immediate values
			int sdt = imm_type_for_opcode(parser->event.inst.opcode);
			int dt_size = sdt_size(sdt);
			if (dt_size != 8) {
				fprintf(stderr, "error: opcode %s cannot accept a string literal immediate value\n",
					name_for_opcode(parser->event.inst.opcode));
				ret = 1;
				break;
			}
		}
		else if (symbol[0] == ':') { // Immediate is label
			if (symbol[1] == '\0') { // Empty label
				fprintf(stderr, "error: empty label in \"%s\" line %d char %d\n",
					parser->filename, parser->tline, parser->tch);
				ret = 1;
				break;
			}
			if (parser->event.type != PET_INST) {
				// @todo: Allow labels to be used for raw data
				fprintf(stderr, "error: unsupported statement type for label in \"%s\" line %d char %d\n",
					parser->filename, parser->tline, parser->tch);
				ret = 1;
				break;
			}
		}
		else {
			// Parse immediate literal as an integer value
			int64_t immval = 0;
			bool success = parse_int(symbol, &immval);
			if (!success) {
				fprintf(stderr, "error: failed to parse integer literal \"%s\" in \"%s\" line %d char %d\n",
					symbol, parser->filename, parser->tline, parser->tch);
				ret = 1;
				break;
			}

			if (parser->ts == PTS_PIMM) {
				// Compact pseudo-ops where possible
				int opcode = parser->event.inst.opcode; // Opcode
				int min_len = min_bytes_for_val(immval);
				switch (opcode) {
				case op_push8as8:
					// Already as compact as possible
					break;
				case op_push16as16:
					if (min_len < 2) {
						if (immval < 0) opcode = op_push8asi16;
						else opcode = op_push8asu16;
					}
					break;
				case op_push32as32:
					if (min_len < 2) {
						if (immval < 0) opcode = op_push8asi32;
						else opcode = op_push8asu32;
					}
					else if (min_len < 3) {
						if (immval < 0) opcode = op_push16asi32;
						else opcode = op_push16asu32;
					}
					break;
				case op_push64as64:
					if (min_len < 2) {
						if (immval < 0) opcode = op_push8asi64;
						else opcode = op_push8asu64;
					}
					else if (min_len < 3) {
						if (immval < 0) opcode = op_push16asi64;
						else opcode = op_push16asu64;
					}
					else if (min_len < 5) {
						if (immval < 0) opcode = op_push32asi64;
						else opcode = op_push32asu64;
					}
					break;
				default:
					fprintf(stderr, "error: invalid opcode for pseudo-op in \"%s\" line %d char %d\n",
							parser->filename, parser->tline, parser->tch);
					ret = 1;
				}
				if (ret) break;
				parser->event.inst.opcode = opcode;
			}

			// Look up immediate type and size
			int sdt;
			if (parser->event.type == PET_DATA) {
				// Raw data size determines immediate data type
				// @todo: Could make cleaner with a function to look up data type for size
				if (parser->event.data.len == 1) {
					sdt = SDT_A8;
				}
				else if (parser->event.data.len == 2) {
					sdt = SDT_A16;
				}
				else if (parser->event.data.len == 4) {
					sdt = SDT_A32;
				}
				else if (parser->event.data.len == 8) {
					sdt = SDT_A64;
				}
				else {
					fprintf(stderr, "error: bad raw data size in \"%s\" line %d char %d\n",
						parser->filename, parser->tline, parser->tch);
					ret = 1;
					break;
				}
			}
			else { // Look up data type based on opcode
				sdt = imm_type_for_opcode(parser->event.inst.opcode);
			}
			int dt_size = sdt_size(sdt);

			// Check numeric literal immediate bounds
			if (dt_size < 8) {
				int64_t minval, maxval;
				sdt_min_max(sdt, &minval, &maxval);
				if (immval < minval || immval > maxval) {
					fprintf(stderr, "error: literal \"%s\" is out of bounds for type in \"%s\" line %d char %d\n",
							symbol, parser->filename, parser->tline, parser->tch);
					ret = 1;
					break;
				}
			}

			// Transform literal value to byte array
			// @todo: Make these cases more similar
			if (parser->event.type == PET_DATA) {
				put_little64((uint64_t)immval, parser->event.data.raw);
			}
		}
		if (parser->event.type == PET_INST) {
			// Emit instruction event with immediate
			if (symbol == parser->token) { // No lookup was performed
				parser->event.inst.imm = parser->token;
				parser->token = NULL;
			}
			else { // Lookup was performed
				parser->event.inst.imm = bstrdupb(symbol);
			}
		}

		parser->ts = PTS_EOL; // Expect end of line
	}	break;

	//
	// Expect end of line
	//
	case PTS_EOL:
		fprintf(stderr, "error: expected eol in \"%s\" line %d char %d\n",
				parser->filename, parser->tline, parser->tch);
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
			parser->defkey = bstrdupb(symbol);
		}
		parser->ts = PTS_DEF_VAL;
		break;
	case PTS_DEF_VAL: // Symbol definition value
		if (symbol == parser->token) { // No lookup was performed
			parser->token = NULL;
		}
		else { // Lookup was performed
			symbol = bstrdupb(symbol);
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
		int64_t litval;
		bool success = parse_int(symbol, &litval);
		if (!success) {
			fprintf(stderr, "error: failed to parse integer literal \"%s\" in \"%s\" line %d char %d\n",
				symbol, parser->filename, parser->tline, parser->tch);
			ret = 1;
			break;
		}

		// Begin section event
		parser->event.type = PET_SECTION;
		parser->event.sec.addr = litval;

		parser->ts = PTS_EOL; // Expect end of line
	}	break;

	//
	// Include file
	//
	case PTS_INCLUDE:
		// Require include file name to be quoted
		if (symbol[0] != '"') {
			fprintf(stderr, "error: expected quoted filename in \"%s\" line %d char %d\n",
				parser->filename, parser->tline, parser->tch);
			ret = 1;
		}
		else { // Emit an include event
			parser->event.type = PET_INCLUDE;
			parser->event.filename = parser->token;
			parser->token = NULL;
		}
		parser->ts = PTS_EOL;
		break;

	default:
		fprintf(stderr, "error: bad token state while parsing \"%s\" line %d char %d\n",
			parser->filename, parser->tline, parser->tch);
		ret = 1;
		break;
	}

	if (parser->token) {
		bfree(parser->token);
		parser->token = NULL;
	}
	return ret;
}

// Finishes the current line. Returns zero on success.
static int parser_finish_line(struct parser *parser, struct parser_event *pe)
{
	if (parser->ts == PTS_EOL) { // Expected EOL
		parser->ts = PTS_DEFAULT;
	}
	if (parser->ts != PTS_DEFAULT) {
		fprintf(stderr, "error: unexpected EOL in \"%s\" line %d char %d\n",
			parser->filename, parser->line, parser->ch);
		return 1;
	}
	if (parser->event.type != PET_NONE) {
		// Emit parser event
		memcpy(pe, &parser->event, sizeof(*pe));
		parser_event_init(&parser->event);
	}
	return 0;
}

enum { // Parser syntax states
	PSS_DEFAULT,
	PSS_QUOTED,
	PSS_ESCAPED,
	PSS_COMMENT,
};

int parser_parse_byte(struct parser *parser, byte b, struct parser_event *pe)
{
	int ret = 0;

	// Decode the given byte
	ucp c;
	ucp *next = utf8_decoder_decode(&parser->decoder, b, &c, &ret);
	if (ret) {
		fprintf(stderr, "error: UTF-8 decoding error in \"%s\" line %d char %d\n",
			parser->filename, parser->line, parser->ch);
		return ret;
	}
	if (next == &c) { // No character generated
		return ret;
	}

	// Re-encode a completed character into a B-string
	bchar *enc = bstrdupu(&c, 1, &ret);
	if (ret) {
		fprintf(stderr, "error: UTF-8 encoding error in \"%s\" line %d char %d\n",
			parser->filename, parser->line, parser->ch);
		return ret;
	}

	// Process the decoded character
	switch (parser->ss) {
	case PSS_DEFAULT:
		if (c == '"') { // Begin quoted token
			ret = parser_finish_token(parser); // First finish any adjacent token
			if (ret) break;
			// Start new token
			parser->token = bstrdupb(enc);
			parser->tline = parser->line;
			parser->tch = parser->ch;
			parser->ss = PSS_QUOTED;
		}
		else if (isspace(c) || c == ';' || c == '\0') { // These finish a token
			ret = parser_finish_token(parser);
			if (ret) break;
			if (c == ';') { // Comments are introduced by ';'
				parser->ss = PSS_COMMENT;
				ret = parser_finish_line(parser, pe);
			}
			else if (c == '\n') { // Line end
				ret = parser_finish_line(parser, pe);
			}
		}
		else { // Anything else continues or starts a token
			if (parser->token == NULL) { // Start a new token
				parser->token = balloc();
				parser->tline = parser->line;
				parser->tch = parser->ch;
			}
			parser->token = bstrcatb(parser->token, enc);
		}
		break;

	case PSS_QUOTED: // Character in string literal
	case PSS_ESCAPED: // Escaped character in string literal
		// Add character to token
		parser->token = bstrcatb(parser->token, enc);
		if (c == '\n') { // Not even escaped newlines are allowed
			fprintf(stderr, "error: newline in string literal in \"%s\" line %d char %d\n",
				parser->filename, parser->tline, parser->tch);
			ret = 1;
		}
		else if (parser->ss == PSS_ESCAPED) { // Escaped characters are not processed further
			parser->ss = PSS_QUOTED;
		}
		else if (c == '"') { // Unescaped '"' ends quoted token
			// Validate any escape sequences in string literal
			bchar *unesc_token = balloc();
			bool result = parse_string_lit(parser->token, &unesc_token);
			bfree(unesc_token);
			if (!result) {
				fprintf(stderr, "error: invalid string literal in \"%s\" line %d char %d\n",
					parser->filename, parser->tline, parser->tch);
				ret = 1;
				break;
			}
			ret = parser_finish_token(parser); // Finish token
			parser->ss = PSS_DEFAULT;
		}
		else if (c == '\\') { // Backslash begins escape sequence
			parser->ss = PSS_ESCAPED;
		}
		break;

	case PSS_COMMENT:
		if (c == '\n') { // Comments are ended by newline
			parser->ss = PSS_DEFAULT;
		}
		break;

	default: // Bad parser syntax state
		fprintf(stderr, "error: bad syntax state while parsing \"%s\" line %d char %d\n",
			parser->filename, parser->tline, parser->tch);
		ret = 1;
		break;
	}
	bfree(enc);

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
