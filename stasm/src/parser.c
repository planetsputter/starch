// parser.c

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "bstr.h"
#include "parser.h"
#include "starch.h"

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
				// @todo: allow hexadecimal notation
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

// Token types
enum {
	// Before second pass
	TT_EOL,
	TT_WORD,
	// After second pass
	TT_OPCODE,
	TT_INT1,
	TT_INT2,
	TT_INT4,
	TT_INT8,
};

//
// Token structure
//
struct Token {
	int type;
	union {
		char *string; // B-string
		long long integer;
	} val;
	int line, col;
	struct Token *next;
};

static void Token_Init(struct Token *token, int line, int col)
{
	memset(token, 0, sizeof(struct Token));
	token->line = line;
	token->col = col;
}

static void Token_Destroy(struct Token *token)
{
	if (token->type == TT_WORD) {
		if (token->val.string) {
			bstr_free(token->val.string);
		}
	}
}

//
// SP structure
//
struct SP {
	char *key, *val; // B-Strings
	struct SP *next;
};

void SP_Init(struct SP *sp)
{
	memset(sp, 0, sizeof(*sp));
}

void SP_Destroy(struct SP *sp)
{
	if (sp->key) {
		bstr_free(sp->key);
	}
	if (sp->val) {
		bstr_free(sp->val);
	}
}

//
// Parser
//

void Parser_Init(struct Parser *parser)
{
	memset(parser, 0, sizeof(struct Parser));
	utf8_decoder_init(&parser->decoder);
	parser->line = 1;
	parser->col = 1;
}

void Parser_Destroy(struct Parser *parser)
{
	// Destroy all tokens in document
	for (struct Token *token = parser->docFront; token;) {
		struct Token *next = token->next;
		Token_Destroy(token);
		free(token);
		token = next;
	}
	// Destroy all symbol string pairs
	for (struct SP *symbol = parser->symbolFront;
		symbol;) {
		struct SP *next = symbol->next;
		SP_Destroy(symbol);
		free(symbol);
		symbol = next;
	}
}

static void Parser_FinishWord(struct Parser *parser)
{
	if (parser->word != NULL) {
		// Add word as token to end of document
		struct Token *newToken = (struct Token*)malloc(sizeof(struct Token));
		Token_Init(newToken, parser->line, parser->col - bstr_len(parser->word));
		newToken->type = TT_WORD;
		newToken->val.string = parser->word;
		if (parser->docBack == NULL) {
			parser->docFront = parser->docBack = newToken;
		}
		else {
			parser->docBack->next = newToken;
			parser->docBack = newToken;
		}
		parser->word = NULL;
	}
}

static void Parser_FinishLine(struct Parser *parser)
{
	// First finish the current word
	Parser_FinishWord(parser);

	// Create EOL token
	struct Token *newToken = (struct Token*)malloc(sizeof(struct Token));
	Token_Init(newToken, parser->line, parser->col);
	newToken->type = TT_EOL;
	if (parser->docBack == NULL) {
		parser->docFront = parser->docBack = newToken;
	}
	else {
		parser->docBack->next = newToken;
		parser->docBack = newToken;
	}
}

// Takes ownership of the given B-string key and adds it to the back of the symbol list
static void Parser_AddSymbolKey(struct Parser *parser, char *key)
{
	struct SP *sp = (struct SP*)malloc(sizeof(struct SP));
	SP_Init(sp);
	sp->key = key;
	if (!parser->symbolBack) {
		parser->symbolFront = parser->symbolBack = sp;
	}
	else {
		parser->symbolBack->next = sp;
		parser->symbolBack = sp;
	}
}

static char *Parser_ValForSymbolKey(struct Parser *parser, const char *key)
{
	char *val = NULL;
	for (struct SP *sp = parser->symbolFront; sp; sp = sp->next) {
		if (strcmp(sp->key, key) == 0) {
			val = sp->val;
			break;
		}
	}
	return val;
}

enum {
	PARSER_STATE_DEFAULT,
	PARSER_STATE_COMMENT,
};

void Parser_ParseByte(struct Parser *parser, byte b, int *error)
{
	*error = 0;

	// Decode the given byte
	ucp c;
	ucp *next = utf8_decoder_decode(&parser->decoder, b, &c, error);
	if (*error || next == &c) { // No character generated
		return;
	}

	// Re-encode a completed character
	byte enc[5];
	*utf8_encode_char(c, enc, sizeof(enc) - 1, error) = '\0';
	if (*error) return;

	// Process the decoded character
	switch (parser->state) {
	case PARSER_STATE_DEFAULT:
		if (isalnum(c) || c == '.' || c == '-' || c == '_' || c == '\'' || c == '\\') { // These continue or start a word
			if (parser->word == NULL) {
				parser->word = bstr_alloc();
			}
			parser->word = bstr_cat(parser->word, (const char*)enc);
		}
		else { // Anything else finishes a word
			Parser_FinishWord(parser);
			if (c == ';') { // Comments are introduced by ';'
				Parser_FinishLine(parser);
				parser->state = PARSER_STATE_COMMENT;
			}
			else if (c == '\n') { // Line end
				Parser_FinishLine(parser);
			}
			else if (!isspace(c)) { // Everything else gets its own word
				parser->word = bstr_alloc();
				parser->word = bstr_cat(parser->word, (const char *)enc);
				Parser_FinishWord(parser);
			}
		}
		break;
	case PARSER_STATE_COMMENT:
		if (c == '\n') { // Comments are ended by newline
			parser->state = PARSER_STATE_DEFAULT;
		}
		break;
	}

	// Keep track of line and column
	if (c == '\n') {
		parser->line++;
		parser->col = 1;
	}
	else {
		parser->col++;
	}
}

int Parser_CanTerminate(struct Parser *parser)
{
	return utf8_decoder_can_terminate(&parser->decoder);
}

// Assembler commands
enum {
	AC_NONE = 0,
	AC_DEFINE_KEY,
	AC_DEFINE_VAL,
};

int Parser_Terminate(struct Parser *parser)
{
	Parser_FinishLine(parser);

	int ret = 0, imm_dt = -1, ac = AC_NONE;
	bool expect_eol = false;

	// At this point all tokens are either of type TT_EOL or TT_WORD.
	// We need to parse the tokens further to interpret instruction opcodes and arguments
	// and to eliminate TT_EOL tokens.
	struct Token *token, *prev, *next;
	for (token = parser->docFront, prev = NULL, next = NULL;
		token != NULL && ret == 0;
		prev = token, token = next) {
		next = token->next;

		if (ac != AC_NONE) {
			if (token->type != TT_WORD) { // Assembler commands expect words
				break;
			}
			long long litval;
			switch (ac) {
			case AC_DEFINE_KEY:
				if (parse_int(token->val.string, &litval)) {
					fprintf(stderr, "error: symbol name cannot be integer literal on line %d\n", token->line);
					ret = 1;
				}
				else {
					Parser_AddSymbolKey(parser, token->val.string);
					token->val.string = NULL;
					ac++;
				}
				break;
			case AC_DEFINE_VAL:
				parser->symbolBack->val = token->val.string;
				token->val.string = NULL;
				expect_eol = true;
				ac = AC_NONE;
				break;
			default:
				fprintf(stderr, "error: invalid assembler command state %d\n", ac);
				ret = 1;
			}
			// Remove the token
			if (prev) {
				prev->next = next;
			}
			else {
				parser->docFront = next;
			}
			if (!next) {
				parser->docBack = prev;
			}
			Token_Destroy(token);
			free(token);
			token = prev;
		}
		else if (imm_dt >= 0) { // Expecting an immediate value
			if (token->type == TT_WORD) {
				// Parse immediate value
				long long litval;
				const char *symVal = NULL;
				bool success = parse_int(token->val.string, &litval);
				if (!success) {
					symVal = Parser_ValForSymbolKey(parser, token->val.string);
					if (symVal) {
						success = parse_int(symVal, &litval);
					}
				}
				if (!success) {
					fprintf(stderr, "error: invalid integer literal \"%s\"", token->val.string);
					if (symVal) {
						fprintf(stderr, " (\"%s\")", symVal);
					}
					fprintf(stderr, " on line %d\n", token->line);
					ret = 1;
				}
				else { // Successfully parsed integer literal. Does it match the expected type?
					// @todo: have a function which returns the min and max values for a given data type
					// @todo: also have a function which returns how many bytes required for a given data type
					switch (imm_dt) {
					case dt_a8:
						if (litval < -0x80 || litval > 0xff) success = false;
						else token->type = TT_INT1;
						break;
					case dt_a16:
						if (litval < -0x8000 || litval > 0xffff) success = false;
						else token->type = TT_INT2;
						break;
					case dt_a32:
						if (litval < -0x80000000l || litval > 0xffffffffl) success = false;
						else token->type = TT_INT4;
						break;
					case dt_a64:
						// @todo: range check?
						token->type = TT_INT8;
						break;
					case dt_i8:
						if (litval < -0x80 || litval > 0x7f) success = false;
						else token->type = TT_INT1;
						break;
					case dt_i16:
						if (litval < -0x8000 || litval > 0x7fff) success = false;
						else token->type = TT_INT2;
						break;
					case dt_i32:
						if (litval < -0x80000000l || litval > 0x7fffffffl) success = false;
						else token->type = TT_INT4;
						break;
					case dt_i64:
						// @todo: range check?
						token->type = TT_INT8;
						break;
					case dt_u8:
						if (litval < 0 || litval > 0xff) success = false;
						else token->type = TT_INT1;
						break;
					case dt_u16:
						if (litval < 0 || litval > 0xffff) success = false;
						else token->type = TT_INT2;
						break;
					case dt_u32:
						if (litval < 0 || litval > 0xffffffffl) success = false;
						else token->type = TT_INT4;
						break;
					case dt_u64:
						// @todo: range check?
						token->type = TT_INT8;
						break;
					default:
						fprintf(stderr, "error: invalid data type expected on line %d\n", token->line);
						ret = 1;
						break;
					}

					if (!success) {
						fprintf(stderr, "error: integer literal \"%s\" out of range\n", token->val.string);
						ret = 1;
					}
					else if (ret == 0) {
						Token_Destroy(token);
						token->val.integer = litval;
					}
				}
				imm_dt = -1;
			}
			else { // Incomplete statement
				break;
			}
		}
		else if (expect_eol && token->type != TT_EOL) {
			fprintf(stderr, "error: expected EOL on line %d col %d\n", token->line, token->col);
			ret = 1;
		}
		else if (token->type == TT_WORD) { // Beginning of statement
			if (token->val.string[0] == '.') {
				// Assembler commands begin with '.'
				if (strcmp(token->val.string, ".define") == 0) {
					ac = AC_DEFINE_KEY;
				}
				else {
					fprintf(stderr, "error: unrecognized assembler command \"%s\"\n", token->val.string);
					ret = 1;
				}
				// Remove the token
				if (prev) {
					prev->next = next;
				}
				else {
					parser->docFront = next;
				}
				if (!next) {
					parser->docBack = prev;
				}
				Token_Destroy(token);
				free(token);
				token = prev;
			}
			else {
				int opcode = opcode_for_name(token->val.string);
				if (opcode < 0) {
					fprintf(stderr, "error: invalid opcode \"%s\" on line %d\n", token->val.string, token->line);
					ret = 1;
				}
				else {
					// Replace word value with integer opcode value
					Token_Destroy(token);
					token->type = TT_OPCODE;
					token->val.integer = opcode;

					// See how many immediate arguments to expect
					int count = imm_count_for_opcode(opcode);
					if (count == 0) {
						expect_eol = true;
					}
					else if (count == 1) {
						// Get immediate argument type
						ret = imm_types_for_opcode(opcode, &imm_dt);
						if (ret) {
							fprintf(stderr, "error: unable to determine immediate types for opcode 0x%02x on line %d\n",
								opcode, token->line);
						}
					}
					else { // For now we only handle zero or one immediate values
						fprintf(stderr, "error: invalid imm count %d for opcode 0x%02x on line %d\n",
							count, opcode, token->line);
						ret = 1;
					}
				}
			}
		}
		else if (token->type == TT_EOL) {
			// Remove EOL tokens
			if (prev) {
				prev->next = next;
			}
			else {
				parser->docFront = next;
			}
			if (!next) {
				parser->docBack = prev;
			}
			Token_Destroy(token);
			free(token);
			token = prev;
			expect_eol = false;
		}
		else {
			fprintf(stderr, "error: invalid token type %d on line %d\n", token->type, token->line);
			ret = 1;
		}
	}

	if ((imm_dt >= 0 || ac != AC_NONE) && ret == 0) {
		fprintf(stderr, "error: unexpected EOL on line %d\n", token ? token->line : 0);
		ret = 1;
	}

	return ret;
}

int Parser_WriteBytecode(struct Parser *parser, FILE *outstream)
{
	int ret = 0;
	for (struct Token *token = parser->docFront; token != NULL && ret == 0; token = token->next) {
		int len = 0;
		switch (token->type) {
		case TT_OPCODE:
		case TT_INT1:
			len = 1;
			break;
		case TT_INT2:
			len = 2;
			break;
		case TT_INT4:
			len = 4;
			break;
		case TT_INT8:
			len = 8;
			break;
		default:
			fprintf(stderr, "error: invalid token type: %d\n", token->type);
			ret = -1;
		}
		for (int i = 0; i < len; i++) {
			unsigned char val = (unsigned char)(token->val.integer >> (i * 8));
			ret = fwrite(&val, 1, 1, outstream);
			if (ret != 1) {
				fprintf(stderr, "error: failed to write to output file\n");
				break;
			}
			ret = 0;
		}
	}
	fflush(outstream);
	return ret;
}
