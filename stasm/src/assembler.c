// assembler.c

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "assembler.h"
#include "bstr.h"
#include "lits.h"
#include "starch.h"
#include "stmsg.h"
#include "util.h"

//
// Automatic symbols
//
struct autosym {
	const char *name;
	uint64_t val;
};
// Automatic symbols, besides instruction opcodes and interrupt numbers, in alphabetic order
static const struct autosym autosyms[] = {
	{ "BEGIN_INT_ADDR", BEGIN_INT_ADDR },
	{ "INIT_PC_VAL", INIT_PC_VAL },
	{ "IO_ASSERT_ADDR", IO_ASSERT_ADDR },
	{ "IO_FLUSH_ADDR", IO_FLUSH_ADDR },
	{ "IO_STDIN_ADDR", IO_STDIN_ADDR },
	{ "IO_STDOUT_ADDR", IO_STDOUT_ADDR },
	{ "IO_URAND_ADDR", IO_URAND_ADDR },
};
// Returns a pointer to the autosym struct for the given name, or NULL
static const struct autosym *get_autosym(const char *name)
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
// Sets *val to token->str if no substitution was performed.
// If token->str begins with '$', attempts to look up a symbol value
// and sets *val to a string from the assembler symbol map on success.
// Returns zero on success and sets *val non-NULL.
// Returns non-zero on failure and sets *val to NULL.
static int symbol_sub(struct assembler *as, struct token *token, bchar **val)
{
	*val = NULL;
	if (token->str[0] != '$') { // No symbolic substitution
		*val = token->str;
	}
	else if (token->str[1] == '\0') { // Check for empty symbol name
		stmsgtf(SMT_ERROR, token->lineno, token->charno, "empty symbol name");
	}
	else {
		// Look up an existing symbol definition
		bchar *name = bstrdupc(token->str + 1);
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
					const struct autosym *as = get_autosym(name);
					if (as) {
						sprintf(symbuf, "%#"PRIx64, as->val);
						symbol = bstrdupc(symbuf);
					}
				}
			}
			if (symbol) { // Found a matching autosymbol
				as->defs = bmap_insert(as->defs, name, symbol);
			}
			else {
				stmsgtf(SMT_ERROR, token->lineno, token->charno, "undefined symbol \"%s\"", name);
				bfree(name);
			}
		}
		else { // Symbol already defined
			bfree(name);
		}
		*val = symbol;
	}
	return *val != NULL ? 0 : 1;
}

//
// Assembler commands and pseudo-ops
//
// Assembler commands and pseudo-ops in alphabetic order
static const char *asm_cmd_names[] = {
	"brz16",
	"brz32",
	"brz64",
	"brz8",
	"data16",
	"data32",
	"data64",
	"data8",
	"define",
	"include",
	"pop16",
	"pop32",
	"pop64",
	"pop8",
	"push16",
	"push32",
	"push64",
	"push8",
	"rjmp",
	"section",
	"store16",
	"store32",
	"store64",
	"store8",
	"strings",
};
// Assembler command symbolic constants in same order as above
enum {
	ASM_CMD_BRZ16,
	ASM_CMD_BRZ32,
	ASM_CMD_BRZ64,
	ASM_CMD_BRZ8,
	ASM_CMD_DATA16,
	ASM_CMD_DATA32,
	ASM_CMD_DATA64,
	ASM_CMD_DATA8,
	ASM_CMD_DEFINE,
	ASM_CMD_INCLUDE,
	ASM_CMD_POP16,
	ASM_CMD_POP32,
	ASM_CMD_POP64,
	ASM_CMD_POP8,
	ASM_CMD_PUSH16,
	ASM_CMD_PUSH32,
	ASM_CMD_PUSH64,
	ASM_CMD_PUSH8,
	ASM_CMD_RJMP,
	ASM_CMD_SECTION,
	ASM_CMD_STORE16,
	ASM_CMD_STORE32,
	ASM_CMD_STORE64,
	ASM_CMD_STORE8,
	ASM_CMD_STRINGS,
};
// Returns the index of the given assembler command, or negative
static int get_asm_cmd(const char *cmd)
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

enum { // Assembler parse states
	APS_DEFAULT,
	APS_INCLUDE1,
	APS_INCLUDE2,
	APS_SECTION1,
	APS_SECTION2,
	APS_DEFINE1,
	APS_DEFINE2,
	APS_DEFINE3,
	APS_STRINGS,
	APS_PSOP1,
	APS_PSOP2,
	APS_OPCODE1,
	APS_OPCODE2,
	APS_WAIT_EOS,
};

void assembler_init(struct assembler *as, FILE *outfile)
{
	as->state = APS_DEFAULT;
	as->outfile = outfile;
	as->defs = bmap_create();
	as->code = 0;
	as->word1 = NULL;
	as->word2 = NULL;
	as->include = NULL;
	expr_parser_init(&as->ep, NULL);
	as->sec_count = 0;
	stub_sec_init(&as->curr_sec, 0, 0, 0);
	as->label_recs = NULL;
}

void assembler_destroy(struct assembler *as)
{
	bmap_delete(as->defs);
	as->defs = NULL;
	if (as->word1) {
		token_free(as->word1);
		as->word1 = NULL;
	}
	if (as->word2) {
		token_free(as->word2);
		as->word2 = NULL;
	}
	if (as->include) {
		token_free(as->include);
		as->include = NULL;
	}
	expr_parser_destroy(&as->ep);
	for (struct label_rec *rec = as->label_recs; rec;) {
		struct label_rec *temp = rec->prev;
		label_rec_destroy(rec);
		free(rec);
		rec = temp;
	}
	as->label_recs = NULL;
}

// Handles definition of the given label at the current position
static int assembler_handle_label_def(struct assembler *as, struct token *token)
{
	if (as->sec_count == 0) {
		stmsgtf(SMT_ERROR, token->lineno, token->charno, "expected section definition before label");
		return 1;
	}

	// Compute current position within section
	long fpos = ftell(as->outfile);
	if (fpos < 0) {
		stmsgf(SMT_ERROR, "failed to get offset in output file, errno %d", errno);
		return 1;
	}
	// Compute label address
	uint64_t addr = fpos - as->curr_sec.fpos + as->curr_sec.addr;

	int ret = 0;
	// Look up any existing label record
	struct label_rec *rec = label_rec_lookup(as->label_recs, false, token->str);
	if (!rec) {
		// Label has not been used before. Add new label record to list.
		struct label_rec *next = (struct label_rec*)malloc(sizeof(struct label_rec));
		label_rec_init(next, false, true, bstrdupb(token->str), addr, fpos, as->sec_count - 1, NULL);
		next->prev = as->label_recs;
		as->label_recs = next;
	}
	else if (rec->defined) {
		// A definition already exists for this label
		stmsgtf(SMT_ERROR, token->lineno, token->charno, "definition already exists for label \"%s\"", token->str);
		ret = 1;
	}
	else {
		// Label has been used but not defined. This is the definition.
		// Apply each of the usages.
		assert(rec->usages);
		rec->defined = true;
		rec->addr = addr; // Address is now known
		rec->fpos = fpos; // File position is now known
		rec->si = as->sec_count - 1; // Section index is now known
		for (struct label_usage *lu = rec->usages; lu; lu = lu->prev) {
			// This may adjust the file position due to compaction of pseudo-ops.
			// It may adjust other things such as label addresses.
			ret = label_usage_apply(lu, as->outfile, rec->addr, as->label_recs);
			if (ret) break;
		}

		if (ret == 0) {
			// Get current file position
			fpos = ftell(as->outfile);
			if (fpos < 0) {
				stmsgf(SMT_ERROR, "failed to get offset in output file, errno %d", errno);
				return 1;
			}

			// Reload section data in case section start file position changed due to compaction of previous section.
			// This will also seek to section start.
			ret = stub_load_section(as->outfile, as->sec_count - 1, &as->curr_sec);
			if (ret) {
				stmsgf(SMT_ERROR, "failed to load section %d in output file, stub error %d", as->sec_count - 1, ret);
				return 1;
			}

			// Return to the original position
			ret = fseek(as->outfile, fpos, SEEK_SET);
			if (ret) {
				stmsgf(SMT_ERROR, "failed to seek in output file, errno %d", errno);
				return 1;
			}
		}
	}
	return ret;
}

// Handles the beginning of a section with the given address at the current position
static int assembler_handle_section(struct assembler *as, uint64_t addr)
{
	int ret = 0;
	if (as->sec_count) { // Save the current section before starting a new one
		ret = stub_save_section(as->outfile, as->sec_count - 1, &as->curr_sec);
	}
	if (ret == 0) {
		// Start a new section. Section address and flags are known but size is unknown at this point.
		stub_sec_init(&as->curr_sec, addr, 0, 0);
		ret = stub_save_section(as->outfile, as->sec_count, &as->curr_sec);
	}
	if (ret) {
		stmsgf(SMT_ERROR, "failed to save stub section %d with error %d", as->sec_count, ret);
		return ret;
	}
	ret = stub_load_section(as->outfile, as->sec_count, &as->curr_sec);
	if (ret) {
		stmsgf(SMT_ERROR, "failed to load stub section %d with error %d", as->sec_count, ret);
		return ret;
	}
	as->curr_sec.fpos = ftell(as->outfile);
	if (as->curr_sec.fpos < 0) {
		stmsgf(SMT_ERROR, "failed to get offset of section %d with error %d", as->sec_count, ret);
		ret = 1;
	}
	else {
		as->sec_count++;
	}
	return ret;
}

// Handles the "strings" assembler command at the current position
static int assembler_handle_strings(struct assembler *as, int lineno)
{
	if (as->sec_count == 0) {
		stmsgtf(SMT_ERROR, lineno, 0, "expected section definition before strings");
		return 1;
	}

	// Emit all string literal data at the current position
	int ret = 0;
	for (struct label_rec *rec = as->label_recs; rec; rec = rec->prev) {
		if (!rec->string_lit) { // This is not a string literal, skip it
			continue;
		}

		// Note current file offset
		long fpos = ftell(as->outfile);
		if (fpos < 0) {
			stmsgf(SMT_ERROR, "failed to seek in output file, errno %d", errno);
			ret = 1;
			break;
		}
		// Compute string content address
		uint64_t addr = fpos - as->curr_sec.fpos + as->curr_sec.addr;

		// Write the string literal contents to the file
		size_t write_len = bstrlen(rec->label) + 1;
		size_t bc = fwrite(rec->label, 1, write_len, as->outfile);
		if (bc != write_len) {
			stmsgf(SMT_ERROR, "failed to write to output file, errno %d", errno);
			ret = 1;
			break;
		}

		// Apply all usages of the string literal
		assert(rec->usages);
		rec->defined = true;
		rec->addr = addr; // Address is now known
		rec->fpos = fpos; // File position is now known
		rec->si = as->sec_count - 1; // Section index is now known
		for (struct label_usage *lu = rec->usages; lu; lu = lu->prev) {
			// This may adjust the file position due to compaction of pseudo-ops
			// It may adjust other things such as label addresses.
			ret = label_usage_apply(lu, as->outfile, rec->addr, as->label_recs);
			if (ret) break;
		}

		if (ret == 0) {
			// Get current file position
			fpos = ftell(as->outfile);
			if (fpos < 0) {
				stmsgf(SMT_ERROR, "failed to get offset in output file, errno %d", errno);
				return 1;
			}

			// Reload section data in case section start file position changed due to compaction of previous section.
			// This will also seek to section start.
			ret = stub_load_section(as->outfile, as->sec_count - 1, &as->curr_sec);
			if (ret) {
				stmsgf(SMT_ERROR, "failed to load section %d in output file, stub error %d", as->sec_count - 1, ret);
				return 1;
			}

			// Return to the original position
			ret = fseek(as->outfile, fpos, SEEK_SET);
			if (ret) {
				stmsgf(SMT_ERROR, "failed to seek in output file, errno %d", errno);
				return 1;
			}
		}
	}
	return ret;
}

// Looks up the value of a label or other named constant
bool assembler_lookup_func(void *userptr, struct token *token, int64_t *intval)
{
	struct assembler *as = (struct assembler*)userptr;
	(void)as;
	(void)token;
	(void)intval;
	// @todo: Allow label value lookup
	return false;
}

// Handles the statement consisting of the given opcode or pseudo-op followed by the given expression.
// eos is the token which ended the statement.
// expr may be NULL if there is no immediate value for this opcode.
static int assembler_handle_opcode(struct assembler *as, bool pseudo_op, int code, const struct expr *expr, struct token *eos)
{
	if (as->sec_count == 0) {
		stmsgtf(SMT_ERROR, eos->lineno, eos->charno,
			"expected section definition before first instruction");
		return 1;
	}

	int opcode_size = 1; // For now we assume opcode size is 1 byte
	int opcode = -1, sdt;
	if (pseudo_op) {
		// For pseudo-ops, put the worst-case (max program size) opcode. These may be compacted later.
		switch (code) {
		case ASM_CMD_STORE16:
		case ASM_CMD_POP16:
			if (expr == NULL) {
				opcode = code == ASM_CMD_STORE16 ? op_store16 : op_pop16;
				sdt = SDT_VOID;
			}
			else {
				opcode = op_storerpop16;
				sdt = SDT_A64; // For address
			}
			break;
		case ASM_CMD_STORE32:
		case ASM_CMD_POP32:
			if (expr == NULL) {
				opcode = code == ASM_CMD_STORE32 ? op_store32 : op_pop32;
				sdt = SDT_VOID;
			}
			else {
				opcode = op_storerpop32;
				sdt = SDT_A64; // For address
			}
			break;
		case ASM_CMD_STORE64:
		case ASM_CMD_POP64:
			if (expr == NULL) {
				opcode = code == ASM_CMD_STORE64 ? op_store64 : op_pop64;
				sdt = SDT_VOID;
			}
			else {
				opcode = op_storerpop64;
				sdt = SDT_A64; // For address
			}
			break;
		case ASM_CMD_STORE8:
		case ASM_CMD_POP8:
			if (expr == NULL) {
				opcode = code == ASM_CMD_STORE8 ? op_store8 : op_pop8;
				sdt = SDT_VOID;
			}
			else {
				opcode = op_storerpop8;
				sdt = SDT_A64; // For address
			}
			break;
		case ASM_CMD_BRZ16:
			opcode = op_rbrz16i32;
			sdt = SDT_I32;
			break;
		case ASM_CMD_BRZ32:
			opcode = op_rbrz32i32;
			sdt = SDT_I32;
			break;
		case ASM_CMD_BRZ64:
			opcode = op_rbrz64i32;
			sdt = SDT_I32;
			break;
		case ASM_CMD_BRZ8:
			opcode = op_rbrz8i32;
			sdt = SDT_I32;
			break;
		case ASM_CMD_DATA16:
			opcode = 0;
			opcode_size = 0;
			sdt = SDT_A16;
			break;
		case ASM_CMD_DATA32:
			opcode = 0;
			opcode_size = 0;
			sdt = SDT_A32;
			break;
		case ASM_CMD_DATA64:
			opcode = 0;
			opcode_size = 0;
			sdt = SDT_A64;
			break;
		case ASM_CMD_DATA8:
			opcode = 0;
			opcode_size = 0;
			sdt = SDT_A8;
			break;
		case ASM_CMD_PUSH16:
			opcode = op_push16as16;
			sdt = SDT_A16;
			break;
		case ASM_CMD_PUSH32:
			opcode = op_push32as32;
			sdt = SDT_A32;
			break;
		case ASM_CMD_PUSH64:
			opcode = op_push64as64;
			sdt = SDT_A64;
			break;
		case ASM_CMD_PUSH8:
			opcode = op_push8as8;
			sdt = SDT_A8;
			break;
		case ASM_CMD_RJMP:
			opcode = op_rjmpi32;
			sdt = SDT_I32;
			break;
		default:
			assert(false);
		}
	}
	else {
		opcode = code;
		sdt = imm_type_for_opcode(code);
	}
	// Prepare bytes to write to output file
	uint8_t buff[9]; // Maximum instruction length is 9 bytes
	buff[0] = opcode;

	// Immediate type has already been computed. Get the immediate size.
	int imm_bytes = sdt_size(sdt);

	if (!expr) {
		if (imm_bytes != 0) {
			stmsgtf(SMT_ERROR, eos->lineno, eos->charno, "expected an expression");
			return 1;
		}
	}
	else if (imm_bytes == 0) {
		stmsgtf(SMT_ERROR, eos->lineno, eos->charno, "unexpected expression");
		return 1;
	}
	else { // Immediate value
		assert(sdt != SDT_VOID);
		int ret = 0;
		int64_t imm_val = 0;
		bchar *ts = expr->op_val->str;
		if (ts[0] == '[') { // Bracket expression
			assert(expr->lhs);

			// Require pseudo-op
			if (!pseudo_op) {
				stmsgtf(SMT_ERROR, expr->op_val->lineno, expr->op_val->charno, "unexpected bracket notation");
				return 1;
			}

			// Check that bracket notation is allowed
			switch (code) {
			case ASM_CMD_PUSH16:
			case ASM_CMD_PUSH32:
			case ASM_CMD_PUSH64:
			case ASM_CMD_PUSH8:
			case ASM_CMD_STORE16:
			case ASM_CMD_POP16:
			case ASM_CMD_STORE32:
			case ASM_CMD_POP32:
			case ASM_CMD_STORE64:
			case ASM_CMD_POP64:
			case ASM_CMD_STORE8:
			case ASM_CMD_POP8:
				// Bracket notation allowed
				break;
			default:
				// Unexpected bracket notation
				stmsgtf(SMT_ERROR, expr->op_val->lineno, expr->op_val->charno, "unexpected bracket notation");
				return 1;
			}

			struct expr *addr_expr;
			bool free_addr_expr = false;
			// Check for simple "SFP" or "SFP +" expressions
			bool basesfp = bstrcmpc(expr->lhs->op_val->str, "SFP") == 0; // Base is "SFP"
			bool baseplus = bstrcmpc(expr->lhs->op_val->str, "+") == 0 && expr->lhs->rhs; // Base is binary "+"
			bool baseminus = bstrcmpc(expr->lhs->op_val->str, "-") == 0 && expr->lhs->rhs; // Base is binary "-"
			bool leftsfp = (baseplus || baseminus) && bstrcmpc(expr->lhs->lhs->op_val->str, "SFP") == 0;
			if (basesfp || leftsfp) {
				// This bracket uses SFP offset
				switch (code) {
				case ASM_CMD_PUSH16:
					opcode = op_loadpopsfp16;
					break;
				case ASM_CMD_PUSH32:
					opcode = op_loadpopsfp32;
					break;
				case ASM_CMD_PUSH64:
					opcode = op_loadpopsfp64;
					break;
				case ASM_CMD_PUSH8:
					opcode = op_loadpopsfp8;
					break;
				case ASM_CMD_STORE16:
				case ASM_CMD_POP16:
					opcode = op_storerpopsfp16;
					break;
				case ASM_CMD_STORE32:
				case ASM_CMD_POP32:
					opcode = op_storerpopsfp32;
					break;
				case ASM_CMD_STORE64:
				case ASM_CMD_POP64:
					opcode = op_storerpopsfp64;
					break;
				case ASM_CMD_STORE8:
				case ASM_CMD_POP8:
					opcode = op_storerpopsfp8;
					break;
				default:
					assert(false);
				}
				// The address is the expression without SFP
				if (basesfp) { // Base is "SFP"
					// Create a "0" token with the same line and character number
					addr_expr = expr_new(token_alloc(bstrdupc("0"),
						expr->lhs->op_val->lineno, expr->lhs->op_val->charno));
					free_addr_expr = true;
				}
				else if (baseminus) { // Base is binary "-"
					// Duplicate the right hand side, then insert a unary "-" to negate
					struct expr *tempe = expr_new(token_dup(expr->lhs->op_val));
					tempe->lhs = expr_dup(expr->lhs->rhs);
					addr_expr = tempe;
					free_addr_expr = true;
				}
				else { // Base is "+"
					addr_expr = expr->lhs->rhs;
				}
			}
			else { // SFP notation is not used
				switch (code) {
				case ASM_CMD_PUSH16:
					opcode = op_loadpop16;
					break;
				case ASM_CMD_PUSH32:
					opcode = op_loadpop32;
					break;
				case ASM_CMD_PUSH64:
					opcode = op_loadpop64;
					break;
				case ASM_CMD_PUSH8:
					opcode = op_loadpop8;
					break;
				case ASM_CMD_STORE16:
				case ASM_CMD_POP16:
					opcode = op_storerpop16;
					break;
				case ASM_CMD_STORE32:
				case ASM_CMD_POP32:
					opcode = op_storerpop32;
					break;
				case ASM_CMD_STORE64:
				case ASM_CMD_POP64:
					opcode = op_storerpop64;
					break;
				case ASM_CMD_STORE8:
				case ASM_CMD_POP8:
					opcode = op_storerpop8;
					break;
				default:
					assert(false);
				}
				// The address is the enclosed expression
				addr_expr = expr->lhs;
			}

			// Emit the instruction to push the address
			// @todo: handle negatives, or maybe change to SF[] notation
			ret = assembler_handle_opcode(as, true, ASM_CMD_PUSH64, addr_expr, eos);
			if (free_addr_expr) {
				expr_destroy(addr_expr);
				free(addr_expr);
			}
			if (ret) return ret;

			// Emit the instruction to load or store a value
			ret = assembler_handle_opcode(as, false, opcode, NULL, eos);
			if (ret) return ret;

			opcode = -1;
			switch (code) { // The pop operations require a pop instruction at the end
			case ASM_CMD_POP16:
				opcode = op_pop16;
				break;
			case ASM_CMD_POP32:
				opcode = op_pop32;
				break;
			case ASM_CMD_POP64:
				opcode = op_pop64;
				break;
			case ASM_CMD_POP8:
				opcode = op_pop8;
				break;
			}
			if (opcode >= 0) {
				ret = assembler_handle_opcode(as, false, opcode, NULL, eos);
			}
			return ret;
		}

		// Not bracket notation
		if (pseudo_op) switch (code) {
		case ASM_CMD_STORE16:
		case ASM_CMD_POP16:
		case ASM_CMD_STORE32:
		case ASM_CMD_POP32:
		case ASM_CMD_STORE64:
		case ASM_CMD_POP64:
		case ASM_CMD_STORE8:
		case ASM_CMD_POP8:
			// These codes require bracket notation if they have an expression
			stmsgtf(SMT_ERROR, expr->op_val->lineno, expr->op_val->charno, "expected a bracket expression");
			return 1;
		default:
			// Continue
		}

		struct label_usage *lu = NULL;
		int imm_bytes_reqd;
		bool string_lit = ts[0] == '"';
		if (string_lit || ts[0] == ':') { // String literal or label
			// Get current file offset
			long current_fo = ftell(as->outfile);
			if (current_fo < 0) {
				stmsgf(SMT_ERROR, "failed to get offset with error %d", errno);
				return 1;
			}
			// Compute current address
			uint64_t curr_addr = as->curr_sec.addr + current_fo - as->curr_sec.fpos;

			// Note usage of label, including whether it is a raw usage
			lu = (struct label_usage*)malloc(sizeof(struct label_usage));
			label_usage_init(lu, current_fo, curr_addr, as->sec_count - 1, opcode_size == 0 ? imm_bytes : 0, pseudo_op, opcode);

			// Label name or contents of string literal
			bchar *contents;
			if (string_lit) { // String literal
				contents = balloc();
				if (!parse_string_lit(ts, &contents)) {
					bfree(contents);
					stmsgtf(SMT_ERROR, expr->op_val->lineno, expr->op_val->charno, "invalid string literal");
					return 1;
				}
			}
			else { // Label
				contents = ts;
			}

			// Find existing label record
			struct label_rec *rec = label_rec_lookup(as->label_recs, string_lit, contents);
			if (!rec) {
				// Label has not been used before. Add new label record to list with usage.
				rec = (struct label_rec*)malloc(sizeof(struct label_rec));
				label_rec_init(rec, string_lit, false, string_lit ? contents : bstrdupb(contents), 0, 0, 0, lu);
				rec->prev = as->label_recs;
				as->label_recs = rec;

				// We don't know the label address, so we can't write the immediate value now
				imm_bytes_reqd = 0;
			}
			else {
				if (string_lit) {
					bfree(contents);
				}
				// Add usage to existing label record
				lu->prev = rec->usages;
				rec->usages = lu;

				// If label address is known, then apply it after writing instruction to file
				if (rec->defined) {
					// Because the label address is known, we can determine the immediate value and bytes required
					if (opcode_size) {
						// Check for jump/branch opcodes that explicitly accept and interpret a label immediate.
						int jmp_br = 0, use_delta = 0;
						opcode_is_jmp_br(buff[0], &jmp_br, &use_delta);
						imm_val = use_delta ? rec->addr - lu->addr : rec->addr;
					}
					else { // Label value is data
						imm_val = rec->addr;
					}
					imm_bytes_reqd = min_bytes_for_val(imm_val);
				}
				else { // Label address is not currently known, so we can't write the immediate value now
					imm_bytes_reqd = 0;
				}
			}
		}
		else { // Integer expression
			// For now, we expect all expressions to be immediately evaluable
			// @todo: Support expressions contaiing string literals and labels
			ret = expr_eval(expr, &imm_val, as, assembler_lookup_func);
			if (ret) {
				return ret;
			}
			imm_bytes_reqd = min_bytes_for_val(imm_val);
		}

		if (pseudo_op && imm_bytes_reqd && opcode_size > 0) {
			// Compact pseudo-ops when immediate value is known
			bool oob = false;
			opcode = assembler_compact_op(code, pseudo_op, imm_val, &oob);
			if (oob) {
				stmsgtf(SMT_ERROR, eos->lineno, eos->charno, "immediate value out of range for opcode");
				return 1;
			}
			if (opcode < 0) {
				stmsgtf(SMT_ERROR, eos->lineno, eos->charno, "unable to compact opcode");
				return 1;
			}
			buff[0] = opcode;
			sdt = imm_type_for_opcode(opcode);
			imm_bytes = sdt_size(sdt);

			if (lu) { // Update opcode for label usage
				lu->opcode = opcode;
			}
		}

		// Check that value fits into buffer
		if (imm_bytes_reqd > imm_bytes) {
			stmsgtf(SMT_ERROR, eos->lineno, eos->charno,
				"immediate value \"%s\" is out of bounds for type", eos->str);
			return 1;
		}

		// Write immediate value to buffer
		put_little64(imm_val, buff + opcode_size);
	}

	// Write instruction buffer to output file
	size_t written = fwrite(buff, 1, opcode_size + imm_bytes, as->outfile);
	if (written != (size_t)(opcode_size + imm_bytes)) {
		stmsgf(SMT_ERROR, "failed to write to output file, errno %d", errno);
		return 1;
	}

	return 0;
}

int assembler_handle_token(struct assembler *as, struct token *token)
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
			token_free(as->word1);
			as->word1 = NULL;
		}
		if (as->word2) {
			token_free(as->word2);
			as->word2 = NULL;
		}
		expr_parser_destroy(&as->ep);
		expr_parser_init(&as->ep, NULL);
		assert(!as->include); // Caller should have checked

		if (symbol[0] == '\n' || symbol[0] == ';') { // Allow empty lines and empty statements
			break;
		}
		if (symbol[0] == ':') { // ':' introduces a label
			if (symbol[1] == '\0') {
				// Label name must not be empty
				stmsgtf(SMT_ERROR, token->lineno, token->charno, "invalid label name");
				ret = 1;
				break;
			}
			struct token *symtok = token_alloc(symbol, token->lineno, token->charno);
			ret = assembler_handle_label_def(as, symtok);
			symtok->str = NULL;
			token_free(symtok);
			break;
		}

		// Check for assembler command
		int code = get_asm_cmd(symbol);
		switch (code) {
		case ASM_CMD_BRZ16:
		case ASM_CMD_BRZ32:
		case ASM_CMD_BRZ64:
		case ASM_CMD_BRZ8:
		case ASM_CMD_DATA16:
		case ASM_CMD_DATA32:
		case ASM_CMD_DATA64:
		case ASM_CMD_DATA8:
		case ASM_CMD_PUSH16:
		case ASM_CMD_PUSH32:
		case ASM_CMD_PUSH64:
		case ASM_CMD_PUSH8:
		case ASM_CMD_STORE16:
		case ASM_CMD_STORE32:
		case ASM_CMD_STORE64:
		case ASM_CMD_STORE8:
		case ASM_CMD_POP16:
		case ASM_CMD_POP32:
		case ASM_CMD_POP64:
		case ASM_CMD_POP8:
		case ASM_CMD_RJMP:
			// Pseudo-ops
			nextstate = APS_PSOP1;
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
			assert(code < 0);
			// Everything else must be an instruction
			code = opcode_for_name(symbol);
			if (code < 0) {
				// Instruction is not an exact match for any opcode
				stmsgtf(SMT_ERROR, token->lineno, token->charno, "unrecognized opcode \"%s\"", symbol);
				nextstate = APS_WAIT_EOS; // Don't attempt to process the rest of the statement
				ret = 1;
				break;
			}
			// Valid Starch opcode
			nextstate = APS_OPCODE1;
		}
		as->code = code;
		break;

	//
	// Expect words, not expressions
	//
	case APS_INCLUDE1:
	case APS_DEFINE1:
	case APS_DEFINE2:
		if (symbol[0] == '\n' || symbol[0] == ';') {
			stmsgtf(SMT_ERROR, token->lineno, token->charno, "unexpected end of statement");
			ret = 1;
			nextstate = APS_DEFAULT;
			break;
		}

		// Absorb token
		if (as->word1) {
			assert(as->word2 == NULL);
			if (symbol == token->str) { // No substitution
				as->word2 = token;
				token = NULL;
			}
			else { // Substitution
				as->word2 = token_alloc(bstrdupb(symbol), token->lineno, token->charno);
			}
		}
		else {
			if (symbol == token->str) { // No substitution
				as->word1 = token;
				token = NULL;
			}
			else { // Substitution
				as->word1 = token_alloc(bstrdupb(symbol), token->lineno, token->charno);
			}
		}

		switch (as->state) {
		case APS_INCLUDE1:
			// Require quoted token
			if (symbol[0] != '"') {
				stmsgtf(SMT_ERROR, as->word1->lineno, as->word1->charno, "expected quoted string");
				ret = 1;
				nextstate = APS_WAIT_EOS;
				break;
			}
			nextstate++;
			break;

		case APS_DEFINE1: {
			// Disallow quoted token, integer literal, or label
			int64_t ival = 0;
			if (symbol[0] == '"' || symbol[0] == ':' || parse_int(symbol, &ival)) {
				stmsgtf(SMT_ERROR, as->word1->lineno, as->word1->charno, "invalid symbol name \"%s\"", symbol);
				ret = 1;
				nextstate = APS_WAIT_EOS;
				break;
			}
			nextstate++;
		}	break;
		case APS_DEFINE2:
			// Allow any token
			nextstate++;
			break;

		default:
			assert(false);
		}
		break;

	//
	// Expect expressions
	//
	case APS_OPCODE1:
	case APS_PSOP1:
	case APS_SECTION1:
		// Attempt to parse expression
		if (symbol[0] != '\n' && symbol[0] != ';') {
			if (symbol != token->str) { // Substitution performed
				bfree(token->str);
				token->str = bstrdupb(symbol);
			}
			// Parse the token as part of the expression
			ret = expr_parser_parse(&as->ep, token);
			if (ret) {
				nextstate = APS_WAIT_EOS;
			}
			token = NULL;
			break;
		}
		// Expression finished
		if (!expr_parser_complete(&as->ep)) { // Partial expression
			stmsgtf(SMT_ERROR, token->lineno, token->charno, "incomplete expression");
			ret = 1;
			nextstate = APS_WAIT_EOS;
			break;
		}
		// Fall-through

	//
	// Expect end of statement
	//
	case APS_INCLUDE2:
	case APS_DEFINE3:
	case APS_STRINGS:
		if (symbol[0] != '\n' && symbol[0] != ';') {
			stmsgtf(SMT_ERROR, token->lineno, token->charno, "expected end of statement");
			ret = 1;
			nextstate = APS_WAIT_EOS;
			break;
		}

		nextstate = APS_DEFAULT;

		switch (as->state) {
		case APS_OPCODE1:
		case APS_PSOP1:
			ret = assembler_handle_opcode(as, as->state == APS_PSOP1, as->code, as->ep.expr, token);
			break;

		case APS_INCLUDE2:
			// Included file name is in word1
			assert(as->include == NULL);

			// Parse string literal
			as->include = token_alloc(balloc(), as->word1->lineno, as->word1->charno);
			if (!parse_string_lit(as->word1->str, &as->include->str)) {
				stmsgtf(SMT_ERROR, as->word1->lineno, as->word1->charno, "invalid string literal");
				ret = 1;
			}
			break;

		case APS_SECTION1:
			// @todo: Support section flags after comma
			// Require expression to be immediately evaluable
			int64_t imm_val = 0;
			ret = expr_eval(as->ep.expr, &imm_val, as, assembler_lookup_func);
			if (ret) {
				break;
			}
			if (imm_val < 0) {
				stmsgtf(SMT_ERROR, token->lineno, token->charno, "section address cannot be negative");
				ret = 1;
				break;
			}
			ret = assembler_handle_section(as, (uint64_t)imm_val);
			break;

		case APS_DEFINE3:
			// Symbol name is in word1 while symbol value is in word2.
			as->defs = bmap_insert(as->defs, as->word1->str, as->word2->str);
			// The symbol map takes ownership of the strings
			as->word1->str = NULL;
			as->word2->str = NULL;
			break;

		case APS_STRINGS:
			ret = assembler_handle_strings(as, token->lineno);
			break;

		default:
			assert(false);
		}
		break;

	case APS_WAIT_EOS: // A parse error has occurred. Ignore further tokens until end of statement.
		if (symbol[0] == '\n' || symbol[0] == ';') {
			nextstate = APS_DEFAULT;
		}
		break;

	default:
		assert(false);
	}
	as->state = nextstate;

	if (token) { // Free token if not absorbed
		token_free(token);
	}

	return ret;
}

struct token *assembler_get_include(struct assembler *as)
{
	struct token *include = as->include;
	as->include = NULL;
	return include;
}

int assembler_finish(struct assembler *as, int lineno, int charno)
{
	if (as->state == APS_WAIT_EOS) {
		// An error has already been logged
		return 1;
	}
	if (as->state != APS_DEFAULT) {
		stmsgtf(SMT_ERROR, lineno, charno, "incomplete statement");
		return 1;
	}

	// Reapply any labels usages that may need to be reapplied
	int ret = 0;
	bool applied_any = true;
	while (ret == 0 && applied_any) {
		applied_any = false;
		for (struct label_rec *rec = as->label_recs; rec; rec = rec->prev) {
			if (!rec->defined) {
				// Undefined labels are an error at this point
				stmsgf(SMT_ERROR, "undefined label \"%s\"", rec->label);
				ret = 1;
			}
			else for (struct label_usage *lu = rec->usages; lu; lu = lu->prev) {
				if (lu->needs_apply) {
					// Applying a label may cause compaction that requires other labels to be re-applied
					int applyret = label_usage_apply(lu, as->outfile, rec->addr, as->label_recs);
					if (ret == 0) {
						ret = applyret;
					}
					applied_any = true;
				}
			}
		}
	}

	return ret;
}

// Returns the pseudo-op which may evaluate to the given opcode or -1
static int assembler_psop_for_op(int opcode)
{
	switch (opcode) {
	case op_rbrz16i8:
	case op_rbrz16i16:
	case op_rbrz16i32:
		return ASM_CMD_BRZ16;
	case op_rbrz32i8:
	case op_rbrz32i16:
	case op_rbrz32i32:
		return ASM_CMD_BRZ32;
	case op_rbrz64i8:
	case op_rbrz64i16:
	case op_rbrz64i32:
		return ASM_CMD_BRZ64;
	case op_rbrz8i8:
	case op_rbrz8i16:
	case op_rbrz8i32:
		return ASM_CMD_BRZ8;
	case op_rjmpi8:
	case op_rjmpi16:
	case op_rjmpi32:
		return ASM_CMD_RJMP;
	case op_push8asu16:
	case op_push8asi16:
	case op_push16as16:
		return ASM_CMD_PUSH16;
	case op_push8asu32:
	case op_push8asi32:
	case op_push16asu32:
	case op_push16asi32:
	case op_push32as32:
		return ASM_CMD_PUSH32;
	case op_push8asu64:
	case op_push8asi64:
	case op_push16asu64:
	case op_push16asi64:
	case op_push32asu64:
	case op_push32asi64:
	case op_push64as64:
		return ASM_CMD_PUSH64;
	case op_push8as8:
		return ASM_CMD_PUSH8;
	default:
		return -1;
	}
}

int assembler_compact_op(int opcode, bool pseudo_op, int64_t imm_val, bool *oob)
{
	int psop = pseudo_op ? opcode : assembler_psop_for_op(opcode);
	opcode = -1;
	int sdt = -1, imm_bytes_reqd = -1;
	*oob = false;
	switch (psop) {
	case ASM_CMD_BRZ16:
		sdt = sdt_icontain(imm_val);
		switch (sdt) {
		case SDT_I8:
			opcode = op_rbrz16i8;
			break;
		case SDT_I16:
			opcode = op_rbrz16i16;
			break;
		case SDT_I32:
			opcode = op_rbrz16i32;
			break;
		case SDT_I64:
			// There is no conditional branch by 64-bit immediate opcode
			*oob = true;
			break;
		default:
			assert(false);
		}
		break;
	case ASM_CMD_BRZ32:
		sdt = sdt_icontain(imm_val);
		switch (sdt) {
		case SDT_I8:
			opcode = op_rbrz32i8;
			break;
		case SDT_I16:
			opcode = op_rbrz32i16;
			break;
		case SDT_I32:
			opcode = op_rbrz32i32;
			break;
		case SDT_I64:
			// There is no conditional branch by 64-bit immediate opcode
			*oob = true;
			break;
		default:
			assert(false);
		}
		break;
	case ASM_CMD_BRZ64:
		sdt = sdt_icontain(imm_val);
		switch (sdt) {
		case SDT_I8:
			opcode = op_rbrz64i8;
			break;
		case SDT_I16:
			opcode = op_rbrz64i16;
			break;
		case SDT_I32:
			opcode = op_rbrz64i32;
			break;
		case SDT_I64:
			// There is no conditional branch by 64-bit immediate opcode
			*oob = true;
			break;
		default:
			assert(false);
		}
		break;
	case ASM_CMD_BRZ8:
		sdt = sdt_icontain(imm_val);
		switch (sdt) {
		case SDT_I8:
			opcode = op_rbrz8i8;
			break;
		case SDT_I16:
			opcode = op_rbrz8i16;
			break;
		case SDT_I32:
			opcode = op_rbrz8i32;
			break;
		case SDT_I64:
			// There is no conditional branch by 64-bit immediate opcode
			*oob = true;
			break;
		default:
			assert(false);
		}
		break;

	case ASM_CMD_DATA16:
	case ASM_CMD_DATA32:
	case ASM_CMD_DATA64:
	case ASM_CMD_DATA8:
		// We don't compact raw data
		break;

	case ASM_CMD_PUSH16:
		imm_bytes_reqd = min_bytes_for_val(imm_val);
		if (imm_bytes_reqd < 2) {
			if (imm_val < 0) opcode = op_push8asi16;
			else opcode = op_push8asu16;
		}
		else {
			opcode = op_push16as16;
		}
		break;
	case ASM_CMD_PUSH32:
		imm_bytes_reqd = min_bytes_for_val(imm_val);
		if (imm_bytes_reqd < 2) {
			if (imm_val < 0) opcode = op_push8asi32;
			else opcode = op_push8asu32;
		}
		else if (imm_bytes_reqd < 3) {
			if (imm_val < 0) opcode = op_push16asi32;
			else opcode = op_push16asu32;
		}
		else {
			opcode = op_push32as32;
		}
		break;
	case ASM_CMD_PUSH64:
		imm_bytes_reqd = min_bytes_for_val(imm_val);
		if (imm_bytes_reqd < 2) {
			if (imm_val < 0) opcode = op_push8asi64;
			else opcode = op_push8asu64;
		}
		else if (imm_bytes_reqd < 3) {
			if (imm_val < 0) opcode = op_push16asi64;
			else opcode = op_push16asu64;
		}
		else if (imm_bytes_reqd < 5) {
			if (imm_val < 0) opcode = op_push32asi64;
			else opcode = op_push32asu64;
		}
		else {
			opcode = op_push64as64;
		}
		break;
	case ASM_CMD_PUSH8:
		// Already as compact as possible
		opcode = op_push8as8;
		break;

	case ASM_CMD_RJMP:
		sdt = sdt_icontain(imm_val);
		switch (sdt) {
		case SDT_I8:
			opcode = op_rjmpi8;
			break;
		case SDT_I16:
			opcode = op_rjmpi16;
			break;
		case SDT_I32:
			opcode = op_rjmpi32;
			break;
		case SDT_I64:
			// There is no relative branch by 64-bit immediate opcode
			*oob = true;
			break;
		default:
			assert(false);
		}
		break;
	default:
		assert(false);
	}
	return opcode;
}
