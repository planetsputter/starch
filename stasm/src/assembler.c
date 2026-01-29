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
		bchar *name = bstrdupc(token + 1);
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
				as->defs = bmap_insert(as->defs, name, symbol);
			}
			else {
				bfree(name);
				stasm_msgf(SMT_ERROR | SMF_USETOK, "undefined symbol \"%s\"", name);
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
	APS_DATA1,
	APS_DATA2,
	APS_STRINGS,
	APS_PSOP1,
	APS_PSOP2,
	APS_PUSH1,
	APS_PUSH2,
	APS_PUSH3,
	APS_PUSH4,
	APS_PUSH5,
	APS_PUSH6,
	APS_PUSH7,
	APS_PUSH8,
	APS_PUSH9,
	APS_STORE1,
	APS_STORE2,
	APS_STORE3,
	APS_STORE4,
	APS_STORE5,
	APS_STORE6,
	APS_STORE7,
	APS_STORE8,
	APS_OPCODE1,
	APS_OPCODE2,
	APS_WAIT_EOL,
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
	as->sec_count = 0;
	as->curr_sec_fo = 0;
	stub_sec_init(&as->curr_sec, 0, 0, 0);
	as->label_recs = NULL;
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
	for (struct label_rec *rec = as->label_recs; rec;) {
		struct label_rec *temp = rec->prev;
		label_rec_destroy(rec);
		free(rec);
		rec = temp;
	}
	as->label_recs = NULL;
}

// Handles definition of the given label at the current position
static int assembler_handle_label_def(struct assembler *as, bchar *token)
{
	if (as->sec_count == 0) {
		stasm_msgf(SMT_ERROR | SMF_USETOK, "expected section definition before label");
		return 1;
	}

	// Compute current position within section
	long fpos = ftell(as->outfile);
	if (fpos < 0) {
		stasm_msgf(SMT_ERROR, "failed to get offset in output file, errno %d", errno);
		return 1;
	}
	// Compute label address
	uint64_t addr = fpos - as->curr_sec_fo + as->curr_sec.addr;

	int ret = 0;
	// Look up any existing label record
	struct label_rec *rec = label_rec_lookup(as->label_recs, false, token);
	if (!rec) {
		// Label has not been used before. Add new label record to list.
		struct label_rec *next = (struct label_rec*)malloc(sizeof(struct label_rec));
		label_rec_init(next, false, true, bstrdupb(token), addr, NULL);
		next->prev = as->label_recs;
		as->label_recs = next;
	}
	else if (rec->defined) {
		// A definition already exists for this label
		stasm_msgf(SMT_ERROR | SMF_USETOK, "definition already exists for label");
		ret = 1;
	}
	else {
		// Label has been used but not defined. This is the definition.
		assert(rec->usages);
		rec->defined = true;
		rec->addr = addr; // Address is now known
		for (struct label_usage *lu = rec->usages; lu; lu = lu->prev) {
			// Apply each of the usages
			ret = label_usage_apply(lu, as->outfile, rec->addr);
			if (ret) break;
		}

		// Seek back to the original position
		int seek_error = fseek(as->outfile, fpos, SEEK_SET);
		if (seek_error) {
			stasm_msgf(SMT_ERROR, "failed to seek in output file, errno %d", errno);
			if (ret == 0) ret = seek_error;
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
		stasm_msgf(SMT_ERROR, "failed to save stub section %d with error %d", as->sec_count, ret);
		return ret;
	}
	ret = stub_load_section(as->outfile, as->sec_count, &as->curr_sec);
	if (ret) {
		stasm_msgf(SMT_ERROR, "failed to load stub section %d with error %d", as->sec_count, ret);
		return ret;
	}
	as->curr_sec_fo = ftell(as->outfile);
	if (as->curr_sec_fo < 0) {
		stasm_msgf(SMT_ERROR, "failed to get offset of section %d with error %d", as->sec_count, ret);
		ret = 1;
	}
	else {
		as->sec_count++;
	}
	return ret;
}

// Handles the "strings" assembler command at the current position
static int assembler_handle_strings(struct assembler *as)
{
	if (as->sec_count == 0) {
		stasm_msgf(SMT_ERROR, "expected section definition before strings");
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
			stasm_msgf(SMT_ERROR, "failed to seek in output file, errno %d", errno);
			ret = 1;
			break;
		}
		// Compute string content address
		uint64_t addr = fpos - as->curr_sec_fo + as->curr_sec.addr;

		// Write the string literal contents to the file
		size_t write_len = bstrlen(rec->label) + 1;
		size_t bc = fwrite(rec->label, 1, write_len, as->outfile);
		if (bc != write_len) {
			stasm_msgf(SMT_ERROR, "failed to write to output file, errno %d", errno);
			ret = 1;
			break;
		}

		// Apply all usages of the string literal
		assert(rec->usages);
		rec->defined = true;
		rec->addr = addr; // Address is now known
		for (struct label_usage *lu = rec->usages; lu; lu = lu->prev) {
			ret = label_usage_apply(lu, as->outfile, addr);
			if (ret) break;
		}

		// Seek to after the string contents
		int seek_error = fseek(as->outfile, fpos + write_len, SEEK_SET);
		if (seek_error) {
			stasm_msgf(SMT_ERROR, "failed to seek in output file, errno %d", errno);
			if (ret == 0) ret = seek_error;
		}
	}
	return ret;
}

// Handles the given token as part of the given opcode or pseudo-op.
// token may be NULL if there is no immediate value for this opcode.
static int assembler_handle_opcode(struct assembler *as, bool pseudo_op, int code, bchar *token)
{
	if (as->sec_count == 0) {
		stasm_msgf(SMT_ERROR, "expected section definition before first instruction");
		return 1;
	}

	int opcode_size = 1; // For now we assume opcode size is 1 byte
	int opcode, sdt;
	if (pseudo_op) {
		// For pseudo-ops, put the worst-case (max program size) opcode
		switch (code) {
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

	if (!token) {
		assert(imm_bytes == 0);
	}
	else { // Immediate value
		int imm_bytes_reqd;
		int64_t imm_val = 0;
		assert(sdt != SDT_VOID);
		bool string_lit = token[0] == '"';
		if (string_lit || token[0] == ':') { // String literal or label
			// Get current file offset
			long current_fo = ftell(as->outfile);
			if (current_fo < 0) {
				stasm_msgf(SMT_ERROR, "failed to get offset with error %d", errno);
				return 1;
			}
			// Compute current address
			uint64_t curr_addr = as->curr_sec.addr + current_fo - as->curr_sec_fo;

			// Note usage of label, including whether it is a raw usage
			struct label_usage *lu = (struct label_usage*)malloc(sizeof(struct label_usage));
			label_usage_init(lu, current_fo, curr_addr, opcode_size == 0 ? imm_bytes : 0);

			// Label name or contents of string literal
			bchar *contents;
			if (string_lit) { // String literal
				contents = balloc();
				if (!parse_string_lit(token, &contents)) {
					bfree(contents);
					stasm_msgf(SMT_ERROR | SMF_USETOK, "invalid string literal");
					return 1;
				}
			}
			else { // Label
				contents = token;
			}

			// Find existing label record
			struct label_rec *rec = label_rec_lookup(as->label_recs, string_lit, contents);
			if (!rec) {
				// Label has not been used before. Add new label record to list with usage.
				rec = (struct label_rec*)malloc(sizeof(struct label_rec));
				label_rec_init(rec, string_lit, false, string_lit ? contents : bstrdupb(contents), 0, lu);
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
		else { // Integer literal
			// @todo: Pass in a token structure instead
			if (token == as->word1) {
				assert(as->pret1);
				imm_val = as->pval1;
			}
			else if (token == as->word2) {
				assert(as->pret2);
				imm_val = as->pval2;
			}
			else {
				assert(false);
			}

			imm_bytes_reqd = min_bytes_for_val(imm_val);
		}

		if (pseudo_op && imm_bytes_reqd) {
			// Compact pseudo-ops when immediate value is known
			int opcode = -1;
			bool oob = false;
			switch (code) {
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
					oob = true;
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
					oob = true;
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
					oob = true;
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
					oob = true;
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
				if (imm_bytes_reqd < 2) {
					if (imm_val < 0) opcode = op_push8asi16;
					else opcode = op_push8asu16;
				}
				else {
					opcode = op_push16as16;
				}
				break;
			case ASM_CMD_PUSH32:
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
					oob = true;
					break;
				default:
					assert(false);
				}
				break;
			default:
				assert(false);
			}
			if (oob) {
				stasm_msgf(SMT_ERROR | SMF_USETOK, "immediate value out of range for opcode");
				return 1;
			}
			if (opcode >= 0) {
				buff[0] = opcode;
				sdt = imm_type_for_opcode(opcode);
				imm_bytes = sdt_size(sdt);
			}
		}

		// Check that value fits into buffer
		if (imm_bytes_reqd > imm_bytes) {
			stasm_msgf(SMT_ERROR | SMF_USETOK, "immediate value \"%s\" is out of bounds for type", token);
			return 1;
		}

		// Write immediate value to buffer
		put_little64(imm_val, buff + opcode_size);
	}

	// Write instruction buffer to output file
	size_t written = fwrite(buff, 1, opcode_size + imm_bytes, as->outfile);
	if (written != (size_t)(opcode_size + imm_bytes)) {
		stasm_msgf(SMT_ERROR, "failed to write to output file, errno %d", errno);
		return 1;
	}

	return 0;
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
			if (symbol[1] == '\0') {
				// Label name must not be empty
				stasm_msgf(SMT_ERROR | SMF_USETOK, "invalid label name");
				ret = 1;
				break;
			}
			ret = assembler_handle_label_def(as, symbol);
			break;
		}

		// Check for assembler command without substitution
		int code = get_asm_cmd(token);
		switch (code) {
		case ASM_CMD_BRZ16:
		case ASM_CMD_BRZ32:
		case ASM_CMD_BRZ64:
		case ASM_CMD_BRZ8:
			nextstate = APS_PSOP1;
			break;
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
		case ASM_CMD_PUSH16:
		case ASM_CMD_PUSH32:
		case ASM_CMD_PUSH64:
		case ASM_CMD_PUSH8:
			nextstate = APS_PUSH1;
			break;
		case ASM_CMD_STORE16:
		case ASM_CMD_STORE32:
		case ASM_CMD_STORE64:
		case ASM_CMD_STORE8:
			nextstate = APS_STORE1;
			break;
		case ASM_CMD_RJMP:
			nextstate = APS_PSOP1;
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
				stasm_msgf(SMT_ERROR | SMF_USETOK, "unrecognized opcode \"%s\"", symbol);
				nextstate = APS_WAIT_EOL; // Don't attempt to process the rest of the line
				ret = 1;
				break;
			}
			// Valid Starch opcode
			int sdt = imm_type_for_opcode(code);
			nextstate = sdt == SDT_VOID ? APS_OPCODE2 : APS_OPCODE1;
			break;
		}
		as->code = code;
		break;

	//
	// Most intermediate tokens
	//
	case APS_DATA1:
	case APS_DEFINE1:
	case APS_DEFINE2:
	case APS_INCLUDE1:
	case APS_OPCODE1:
	case APS_PSOP1:
	case APS_PUSH1:
	case APS_PUSH3:
	case APS_PUSH4:
	case APS_PUSH6:
	case APS_PUSH7:
	case APS_PUSH8:
	case APS_SECTION1:
	case APS_STORE2:
	case APS_STORE3:
	case APS_STORE5:
	case APS_STORE6:
	case APS_STORE7:
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
				as->word2 = token;
				token = NULL;
			}
			else { // Substitution
				as->word2 = bstrdupb(symbol);
			}
		}
		else {
			if (symbol == token) { // No substitution
				as->word1 = token;
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
				nextstate = APS_WAIT_EOL;
				break;
			}
			nextstate++;
			break;
		case APS_DEFINE1:
			// Disallow quoted token
			if (symbol[0] == '"') {
				stasm_msgf(SMT_ERROR | SMF_USETOK, "unexpected quoted string");
				ret = 1;
				nextstate = APS_WAIT_EOL;
				break;
			}
			nextstate++;
			break;

		case APS_PUSH1:
		case APS_PUSH3:
		case APS_PUSH6:
		case APS_STORE2:
		case APS_STORE5:
			bool done = false;
			switch (as->state) {
			case APS_PUSH1:
				if (symbol[0] == '[') { // Begin bracket notation
					bfree(as->word1);
					as->word1 = NULL;
					done = true;
					nextstate += 2;
					break;
				}
				break;
			case APS_PUSH3:
			case APS_STORE2:
				if (bstrcmpc(symbol, "SFP") == 0) { // Allow "SFP" to indicate SFP addressing mode
					nextstate += 3;
					bfree(as->word1);
					as->word1 = NULL;
					done = true;
					break;
				}
				break;
			case APS_PUSH6:
			case APS_STORE5:
				if (symbol[0] == ']') { // "[SFP]" indicates zero offset
					nextstate += 3;
					bfree(as->word1);
					as->word1 = NULL;
					done = true;
				}
				else {
					nextstate++;
					if (bstrcmpc(symbol, "+") == 0) { // Allow optional "+" after "SFP" before offset
						bfree(as->word1);
						as->word1 = NULL;
						done = true;
					}
				}
				break;
			default:
				assert(false);
			}
			if (done) break;
			// Fall-through
		case APS_PUSH7:
		case APS_OPCODE1:
		case APS_PSOP1:
		case APS_DATA1:
		case APS_STORE6:
			// Allow quoted tokens and labels
			if (symbol[0] == '"' || symbol[0] == ':') {
				nextstate++;
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
				nextstate = APS_WAIT_EOL;
				ret = 1;
				break;
			}
			// Fall-through
		case APS_DEFINE2:
			// Allow any token
			nextstate++;
			break;

		case APS_PUSH4:
		case APS_PUSH8:
		case APS_STORE3:
		case APS_STORE7:
			if (symbol[0] != ']') {
				stasm_msgf(SMT_ERROR | SMF_USETOK, "expected \"]\"");
				nextstate = APS_WAIT_EOL;
				ret = 1;
				break;
			}
			nextstate++;
			break;
		default:
			assert(false);
		}
		break;

	//
	// Last token (end of line)
	//
	case APS_DATA2:
	case APS_DEFINE3:
	case APS_INCLUDE2:
	case APS_OPCODE2:
	case APS_PSOP2:
	case APS_PUSH2:
	case APS_PUSH5:
	case APS_PUSH9:
	case APS_SECTION2:
	case APS_STORE4:
	case APS_STORE8:
	case APS_STRINGS:
		// Expect end of line
		if (token[0] != '\n') {
			stasm_msgf(SMT_ERROR | SMF_USETOK, "expected eol");
			ret = 1;
			nextstate = APS_WAIT_EOL;
			break;
		}
		nextstate = APS_DEFAULT;

		switch (as->state) {
		case APS_DEFINE3:
			// Symbol name is in word1 while symbol value is in word2.
			as->defs = bmap_insert(as->defs, as->word1, as->word2);
			// The symbol map takes ownership of the strings
			as->word1 = NULL;
			as->word2 = NULL;
			break;

		case APS_INCLUDE2:
			// Included file name is in word1
			assert(as->include == NULL);
			as->include = as->word1;
			as->word1 = NULL;
			break;

		case APS_DATA2:
		case APS_OPCODE2:
		case APS_PSOP2:
		case APS_PUSH2:
			// word1 will be NULL if the opcode expects no immediate value.
			// Otherwise it will be a quoted string, label, or integer literal.
			ret = assembler_handle_opcode(as, as->state != APS_OPCODE2, as->code, as->word1);
			break;

		case APS_PUSH5:
		case APS_PUSH9:
			if (!as->word1) { // Such as for "[SFP]" notation
				// @todo: This could be improved with changes to assembler_handle_opcode()
				as->word1 = balloc();
				as->pret1 = true;
				as->pval1 = 0;
			}
			ret = assembler_handle_opcode(as, true, ASM_CMD_PUSH64, as->word1);
			if (ret) break;

			int opcode = -1;
			if (as->state == APS_PUSH5) switch (as->code) {
			case ASM_CMD_PUSH16: opcode = op_loadpop16; break;
			case ASM_CMD_PUSH32: opcode = op_loadpop32; break;
			case ASM_CMD_PUSH64: opcode = op_loadpop64; break;
			case ASM_CMD_PUSH8: opcode = op_loadpop8; break;
			default: assert(false);
			}
			else switch (as->code) {
			case ASM_CMD_PUSH16: opcode = op_loadpopsfp16; break;
			case ASM_CMD_PUSH32: opcode = op_loadpopsfp32; break;
			case ASM_CMD_PUSH64: opcode = op_loadpopsfp64; break;
			case ASM_CMD_PUSH8: opcode = op_loadpopsfp8; break;
			default: assert(false);
			}
			ret = assembler_handle_opcode(as, false, opcode, NULL);
			break;

		case APS_SECTION2:
			// Section address is in word1
			// @todo: Allow section flags
			if (as->pret1) { // Address parsed successfully
				if (as->pval1 < 0) {
					stasm_msgf(SMT_ERROR | SMF_USETOK, "section address cannot be negative");
					ret = 1;
					break;
				}
				ret = assembler_handle_section(as, (uint64_t)as->pval1);
			}
			break;

		case APS_STORE4:
		case APS_STORE8:
			if (!as->word1) { // Such as for "[SFP]" notation
				// @todo: This could be improved with changes to assembler_handle_opcode()
				as->word1 = balloc();
				as->pret1 = true;
				as->pval1 = 0;
			}
			ret = assembler_handle_opcode(as, true, ASM_CMD_PUSH64, as->word1);
			if (ret) break;

			if (as->state == APS_STORE4) switch (as->code) {
			case ASM_CMD_STORE16: opcode = op_storerpop16; break;
			case ASM_CMD_STORE32: opcode = op_storerpop32; break;
			case ASM_CMD_STORE64: opcode = op_storerpop64; break;
			case ASM_CMD_STORE8: opcode = op_storerpop8; break;
			default: assert(false);
			}
			else switch (as->code) {
			case ASM_CMD_STORE16: opcode = op_storerpopsfp16; break;
			case ASM_CMD_STORE32: opcode = op_storerpopsfp32; break;
			case ASM_CMD_STORE64: opcode = op_storerpopsfp64; break;
			case ASM_CMD_STORE8: opcode = op_storerpopsfp8; break;
			default: assert(false);
			}
			ret = assembler_handle_opcode(as, false, opcode, NULL);
			break;

		case APS_STRINGS:
			ret = assembler_handle_strings(as);
			break;

		default:
			assert(false);
		}
		break;

	case APS_STORE1:
		// Optional newline
		if (token[0] == '\n') {
			// With no operand, the "store" commands evaluate to a single instruction
			int opcode;
			switch (as->code) {
			case ASM_CMD_STORE16: opcode = op_store16; break;
			case ASM_CMD_STORE32: opcode = op_store32; break;
			case ASM_CMD_STORE64: opcode = op_store64; break;
			case ASM_CMD_STORE8: opcode = op_store8; break;
			default:
				assert(false);
			}
			ret = assembler_handle_opcode(as, false, opcode, NULL);
			nextstate = APS_DEFAULT;
			break;
		}
		// With an operand, the "store" commands accept bracket notation
		if (token[0] != '[') {
			stasm_msgf(SMT_ERROR | SMF_USETOK, "expected '[' or eol");
			ret = 1;
			nextstate = APS_WAIT_EOL;
			break;
		}
		// Begin bracket notation for store command
		nextstate = APS_STORE2;
		break;

	case APS_WAIT_EOL: // An error has occurred. Ignore further tokens until EOL.
		if (token[0] == '\n') {
			nextstate = APS_DEFAULT;
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

bool assembler_finish(struct assembler *as)
{
	return as->state == APS_DEFAULT;
}
