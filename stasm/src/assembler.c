// assembler.c

#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "assembler.h"
#include "bstr.h"
#include "lits.h"
#include "starch.h"
#include "stasm.h"
#include "util.h"

//
// Automatic symbols
//
struct autosym {
	const char *name;
	uint64_t val;
};
// Automatic symbols, besides instruction opcodes and interrupt numbers, in alphabetic order
static struct autosym autosyms[] = {
	{ "BEGIN_INT_ADDR", BEGIN_INT_ADDR },
	{ "BEGIN_IO_ADDR", BEGIN_IO_ADDR },
	{ "INIT_PC_VAL", INIT_PC_VAL },
	{ "IO_ASSERT_ADDR", IO_ASSERT_ADDR },
	{ "IO_FLUSH_ADDR", IO_FLUSH_ADDR },
	{ "IO_STDIN_ADDR", IO_STDIN_ADDR },
	{ "IO_STDOUT_ADDR", IO_STDOUT_ADDR },
	{ "IO_URAND_ADDR", IO_URAND_ADDR },
};
// Returns a pointer to the autosym struct for the given name, or NULL
static struct autosym *get_autosym(const char *name)
{
	// Use binary search for efficiency
	int low = 0, high = sizeof(autosyms) / sizeof(*autosyms) - 1, mid;
	while (low <= high) {
		mid = (low + high) / 2;
		int comp = strcmp(name, autosyms[mid].name);
		if (comp < 0) {
			high = mid - 1;
		}
		else if (comp > 0) {
			low = mid + 1;
		}
		else {
			return autosyms + mid;
		}
	}
	return NULL;
}

// Performs symbolic substitution on the given token using manual and automatic symbols.
// Sets *val to token if no substitution was performed.
// If token begins with '$', attempts to look up a symbol value
// and sets *val to a string in the assembler symbol map on success.
// Returns zero on success and sets *val non-NULL.
// Returns non-zero on failure and sets *val to NULL.
static int symbol_sub(struct assembler *as, bchar *token, bchar **val)
{
	*val = NULL;
	if (token[0] != '$') { // No symbolic substitution
		*val = token;
	}
	else if (token[1] == '\0') { // Check for empty symbol name
		stasm_msgf(SMT_ERROR | SMF_USETOK, "empty symbol name");
	}
	else {
		// Look up an existing symbol definition
		const char *name = token + 1;
		bchar *symbol = NULL;
		bmap_get(as->defs, name, &symbol);

		if (!symbol) {
			// Check for automatic opcode symbols
			enum { MAX_OPCODE_NAME_LEN = 32 };
			char symbuf[MAX_OPCODE_NAME_LEN + 1];
			if (strncmp(name, "OP_", 3) == 0) {
				// Verify all letters are uppercase and length is below limit
				const char *s;
				for (s = name + 3; isupper(*s) || isdigit(*s); s++);
				if (*s == '\0' && (s - name - 3) <= MAX_OPCODE_NAME_LEN) {
					// All letters are uppercase. Convert to lowercase.
					strncpy(symbuf, name + 3, MAX_OPCODE_NAME_LEN);
					symbuf[MAX_OPCODE_NAME_LEN] = '\0';
					for (char *sb = symbuf; *sb != '\0'; sb++) {
						*sb = tolower(*sb);
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
				int stint = stint_for_name(name);
				if (stint >= 0) { // Interrupt name
					sprintf(symbuf, "%d", stint);
					symbol = bstrdupc(symbuf);
				}
				else {
					// Look up other automatic symbols
					struct autosym *as = get_autosym(name);
					if (as) {
						sprintf(symbuf, "%#"PRIx64, as->val);
						symbol = bstrdupc(symbuf);
					}
				}
			}
			if (symbol) { // Found a matching autosymbol
				bmap_insert(as->defs, bstrdupc(name), symbol);
			}
			else {
				stasm_msgf(SMT_ERROR | SMF_USETOK, "undefined symbol \"%s\"", name);
			}
		}
		*val = symbol;
	}
	return *val != NULL ? 0 : 1;
}

//
// Assembler commands
//
// Assembler commands in alphabetic order
static const char *asm_cmd_names[] = {
	"data16",
	"data32",
	"data64",
	"data8",
	"define",
	"include",
	"section",
	"strings",
};
// Assembler command symbolic constants in same order as above
enum {
	ASM_CMD_DATA16,
	ASM_CMD_DATA32,
	ASM_CMD_DATA64,
	ASM_CMD_DATA8,
	ASM_CMD_DEFINE,
	ASM_CMD_INCLUDE,
	ASM_CMD_SECTION,
	ASM_CMD_STRINGS,
};
// Returns the index of the given assembler command, or negative
int get_asm_cmd(const char *cmd)
{
	// Use binary search for efficiency
	int low = 0, high = sizeof(asm_cmd_names) / sizeof(*asm_cmd_names) - 1, mid;
	while (low <= high) {
		mid = (low + high) / 2;
		int comp = strcmp(cmd, asm_cmd_names[mid]);
		if (comp < 0) {
			high = mid - 1;
		}
		else if (comp > 0) {
			low = mid + 1;
		}
		else {
			return mid;
		}
	}
	return -1;
}

//
// Pseudo-ops
//
// Pseudo-op names in alphabetic order
static const char *psop_names[] = {
	"push16",
	"push32",
	"push64",
	"push8",
};
// Pseudo-op symbolic constants in same order as above
enum {
	PSOP_PUSH16,
	PSOP_PUSH32,
	PSOP_PUSH64,
	PSOP_PUSH8,
};
// Returns the index of the given pseudo-op, or negative
int get_psop(const char *name)
{
	// Use binary search for efficiency
	int low = 0, high = sizeof(psop_names) / sizeof(*psop_names) - 1, mid;
	while (low <= high) {
		mid = (low + high) / 2;
		int comp = strcmp(name, psop_names[mid]);
		if (comp < 0) {
			high = mid - 1;
		}
		else if (comp > 0) {
			low = mid + 1;
		}
		else {
			return mid;
		}
	}
	return -1;
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

enum { // Assembler parse states
	APS_DEFAULT,
	APS_INCLUDE1,
	APS_INCLUDE2,
	APS_SECTION1,
	APS_SECTION2,
	APS_DEFINE1,
	APS_DEFINE2,
	APS_DEFINE3,
	APS_DATA1,
	APS_DATA2,
	APS_STRINGS,
	APS_PUSH1,
	APS_PUSH2,
	APS_OPCODE1,
	APS_OPCODE2,
};

void assembler_init(struct assembler *as, FILE *outfile)
{
	as->state = APS_DEFAULT;
	as->outfile = outfile;
	as->defs = bmap_create();
	as->code = 0;
	as->sdt = SDT_VOID;
	as->word1 = NULL;
	as->word2 = NULL;
	as->include = NULL;
	as->sec_count = 0;
	as->curr_sec_fo = 0;
	stub_sec_init(&as->curr_sec, 0, 0, 0);
}

void assembler_destroy(struct assembler *as)
{
	bmap_delete(as->defs);
	as->defs = NULL;
	bfree(as->word1);
	as->word1 = NULL;
	bfree(as->word2);
	as->word2 = NULL;
	bfree(as->include);
	as->include = NULL;
}

int assembler_handle_token(struct assembler *as, bchar *token)
{
	// Perform symbolic substitution
	bchar *symbol = NULL;
	int ret = symbol_sub(as, token, &symbol);

	int nextstate = as->state;
	if (ret == 0) switch (as->state) {
	//
	// Default state (first token)
	//
	case APS_DEFAULT:
		// Clean up any previous state
		if (as->word1) {
			bfree(as->word1);
			as->word1 = NULL;
		}
		if (as->word2) {
			bfree(as->word2);
			as->word2 = NULL;
		}
		if (as->include) {
			bfree(as->include);
			as->include = NULL;
		}

		if (token[0] == '\n') { // Allow empty lines
			break;
		}
		if (symbol[0] == ':') { // ':' introduces a label
			if (strchr(symbol + 1, ':') != NULL) { // Label name may not contain ':'
				stasm_msgf(SMT_ERROR | SMF_USETOK, "invalid label name");
				ret = 1;
				break;
			}
			// @todo: define label
			break;
		}

		// Check for assembler command without substitution
		switch (get_asm_cmd(token)) {
		case ASM_CMD_DATA16:
		case ASM_CMD_DATA32:
		case ASM_CMD_DATA64:
		case ASM_CMD_DATA8:
			nextstate = APS_DATA1;
			break;
		case ASM_CMD_DEFINE:
			nextstate = APS_DEFINE1;
			break;
		case ASM_CMD_INCLUDE:
			nextstate = APS_INCLUDE1;
			break;
		case ASM_CMD_SECTION:
			nextstate = APS_SECTION1;
			break;
		case ASM_CMD_STRINGS:
			nextstate = APS_STRINGS;
			break;
		default:
			// Everything else must be an instruction
			int code = opcode_for_name(symbol);
			if (code < 0) {
				// Instruction is not an exact match for any opcode. Check for pseudo-ops.
				code = get_psop(symbol);
				switch (code) {
				case PSOP_PUSH16:
				case PSOP_PUSH32:
				case PSOP_PUSH64:
				case PSOP_PUSH8:
					nextstate = APS_PUSH1;
					break;
				default:
					stasm_msgf(SMT_ERROR | SMF_USETOK, "unrecognized opcode \"%s\"", symbol);
					ret = 1;
					break;
				}
			}
			else {
				as->sdt = imm_type_for_opcode(code);
				nextstate = as->sdt == SDT_VOID ? APS_OPCODE2 : APS_OPCODE1;
			}
			as->code = code;
			break;
		}
		break;

	//
	// Most intermediate tokens
	//
	case APS_DATA1:
	case APS_DEFINE1:
	case APS_DEFINE2:
	case APS_INCLUDE1:
	case APS_OPCODE1:
	case APS_PUSH1:
	case APS_SECTION1:
		// Disallow newlines
		if (token[0] == '\n') {
			stasm_msgf(SMT_ERROR, "unexpected newline", symbol);
			nextstate = APS_DEFAULT;
			ret = 1;
			break;
		}

		// Absorb token
		if (as->word1) {
			assert(as->word2 == NULL);
			if (symbol == token) { // No substitution
				as->word2 = symbol;
				token = NULL;
			}
			else { // Substitution
				as->word2 = bstrdupb(symbol);
			}
		}
		else {
			if (symbol == token) { // No substitution
				as->word1 = symbol;
				token = NULL;
			}
			else { // Substitution
				as->word1 = bstrdupb(symbol);
			}
		}

		// Check token type
		switch (as->state) {
		case APS_INCLUDE1:
			// Require quoted token
			if (symbol[0] != '"') {
				stasm_msgf(SMT_ERROR | SMF_USETOK, "expected quoted string");
				ret = 1;
				break;
			}
			break;
		case APS_DEFINE1:
			// Disallow quoted token
			if (symbol[0] == '"') {
				stasm_msgf(SMT_ERROR | SMF_USETOK, "unexpected quoted string");
				ret = 1;
			}
			break;
		case APS_OPCODE1:
		case APS_PUSH1:
		case APS_DATA1:
			// Allow quoted tokens and labels
			if (symbol[0] == '"' || symbol[0] == ':') {
				break;
			}
			// Fall-through
		case APS_SECTION1:
			// Require integer literal
			bool bret;
			if (as->word2) {
				bret = as->pret2 = parse_int(symbol, &as->pval2);
			}
			else {
				bret = as->pret1 = parse_int(symbol, &as->pval1);
			}
			if (!bret) {
				stasm_msgf(SMT_ERROR | SMF_USETOK, "invalid integer literal");
				ret = 1;
			}
			break;
		case APS_DEFINE2:
			// Allow any token
			break;
		default:
			assert(false);
		}
		nextstate++;
		break;

	//
	// Last token (end of line)
	//
	case APS_DATA2:
	case APS_DEFINE3:
	case APS_INCLUDE2:
	case APS_OPCODE2:
	case APS_PUSH2:
	case APS_SECTION2:
	case APS_STRINGS:
		// Expect end of line
		if (token[0] != '\n') {
			stasm_msgf(SMT_ERROR | SMF_USETOK, "expected eol");
			ret = 1;
			break;
		}
		nextstate = APS_DEFAULT;

		switch (as->state) {
		case APS_DEFINE3:
			// Symbol name is in word1 while symbol value is in word2.
			// If word1 is quoted an error message has been emitted.
			if (as->word1[0] != '"') {
				as->defs = bmap_insert(as->defs, as->word1, as->word2);
				// The symbol map takes ownership of the strings
				as->word1 = NULL;
				as->word2 = NULL;
			}
			break;

		case APS_INCLUDE2:
			// Included file name is in word1. If not quoted, an error message has ben emitted.
			if (as->word1[0] == '"') {
				assert(as->include == NULL);
				as->include = as->word1;
				as->word1 = NULL;
			}
			break;

		case APS_OPCODE2:
		case APS_PUSH2:
		case APS_DATA2:
			// @todo
			break;

		case APS_SECTION2:
			// Section address is in word1
			// @todo: Allow section flags
			if (as->pret1) { // Address parsed successfully
				if (as->sec_count) { // Save the current section before starting a new one
					ret = stub_save_section(as->outfile, as->sec_count - 1, &as->curr_sec);
				}
				if (ret == 0) {
					// Start a new section. Section address and flags are known but size is unknown at this point.
					stub_sec_init(&as->curr_sec, (uint64_t)as->pval1, 0, 0);
					ret = stub_save_section(as->outfile, as->sec_count, &as->curr_sec);
				}
				if (ret) {
					stasm_msgf(SMT_ERROR, "failed to save stub section %d with error %d", as->sec_count, ret);
					break;
				}
				ret = stub_load_section(as->outfile, as->sec_count, &as->curr_sec);
				if (ret) {
					stasm_msgf(SMT_ERROR, "failed to load stub section %d with error %d", as->sec_count, ret);
					break;
				}
				as->curr_sec_fo = ftell(as->outfile);
				if (as->curr_sec_fo < 0) {
					stasm_msgf(SMT_ERROR, "failed to get offset of section %d with error %d", as->sec_count, ret);
					ret = 1;
				}
				else {
					as->sec_count++;
				}
			}
			break;

		case APS_STRINGS:
			// @todo: insert string table
			break;

		default:
			assert(false);
		}
		break;

	default:
		assert(false);
	}
	as->state = nextstate;

	if (token) { // Free token if not absorbed
		bfree(token);
	}

	return ret;
}

void assembler_get_include(struct assembler *as, bchar **filename)
{
	*filename = as->include;
	as->include = NULL;
}
