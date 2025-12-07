// assembler.c

#include "assembler.h"
#include "stasm.h"

void assembler_init(struct assembler *as, bchar *filename)
{
	memset(as, 0, sizeof(*as));
	as->outfilename = filename;
	utf8_decoder_init(&as->decoder);
	tokenizer_init(&as->tokenizer);
}

void assembler_destroy(struct assembler *as)
{
	if (as->outfilename) {
		bfree(as->outfilename);
		as->outfilename = NULL;
	}
	tokenizer_destroy(&as->tokenizer);
}

// Parses the given B-string token at *token, which must not be NULL.
// Sets *token to NULL if the token was absorbed. Otherwise the caller retains ownership.
// Sets *error to zero if no error occurs.
static void assembler_handle_token(struct assembler *as, bchar **token, int *error)
{
	// Perform symbolic substitution
	int *error = 0;
	bchar *symbol = NULL;
	if (*token[0] == '$') {
		if (*token[1] == '\0') { // Check for empty symbol name
			stasm_msgf(SMT_ERROR | SMF_USETOK, "empty symbol name");
			*error = 1;
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
					stasm_msgf(SMT_ERROR | SMF_USETOK, "undefined symbol \"%s\"", parser->token + 1);
					ret = 1;
				}
			}
		}
	}
	else {
		symbol = parser->token;
	}
}

void assembler_parse_byte(struct assembler *as, byte b, int *error)
{
	// Decode the byte stream into characters
	ucp c;
	ucp *cp = utf8_decoder_decode(&as->decoder, b, &c, error);
	if (cp != &c + 1 || *error) return; // Error or no byte generated

	// Generate tokens from each character
	tokenizer_parse(&as->tokenizer, c);

	// Handle each token. A single character may generate more than one token.
	bchar *token;
	tokenizer_emit(&as->tokenizer, &token);
	while (token) {
		assembler_handle_token(as, &token, error);
		if (token) {
			bfree(token);
		}
		if (*error) {
			break;
		}
		tokenizer_emit(&as->tokenizer, &token);
	}
}

void assembler_parse_string(struct assembler *as, const char *str, int *error)
{
	for (; *str != '\0'; str++) {
		assembler_parse_byte(as, *str, error);
		if (error) {
			break;
		}
	}
}

void assembler_get_include(struct assembler *as, bchar **filename)
{
	*filename = NULL;
}
