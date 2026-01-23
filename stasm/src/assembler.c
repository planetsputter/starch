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
			if (ret) {
				break;
			}
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

// Handles an opcode, pseudo-op, or data statement
static int assembler_handle_opcode(struct assembler *as)
{
	if (as->sec_count == 0) {
		stasm_msgf(SMT_ERROR, "expected section definition before first instruction");
		return 1;
	}

	// Immediate type has already been computed. Get the immediate size.
	int imm_bytes = sdt_size(as->sdt);

	// Prepare bytes to write to output file
	int opcode_size = 1; // For now we assume opcode size is 1 byte
	uint8_t buff[9]; // Maximum instruction length is 9 bytes
	switch (as->state) {
	case APS_DATA2:
		opcode_size = 0;
		break;
	case APS_OPCODE2:
		buff[0] = as->code;
		break;
	case APS_PUSH2:
		// Put the worst-case (max program size) opcode
		int opcode;
		switch (as->code) {
		case PSOP_PUSH16:
			opcode = op_push16as16;
			break;
		case PSOP_PUSH32:
			opcode = op_push32as32;
			break;
		case PSOP_PUSH64:
			opcode = op_push64as64;
			break;
		case PSOP_PUSH8:
			opcode = op_push8as8;
			break;
		default:
			assert(false);
		}
		buff[0] = opcode;
		break;
	default:
		assert(false);
	}

	if (!as->word1) { // No immediate value
		assert(as->sdt == SDT_VOID && as->state == APS_OPCODE2);
	}
	else { // Immediate value
		int imm_bytes_reqd;
		int64_t imm_val = 0;
		assert(as->sdt != SDT_VOID);
		if (as->word1[0] == '"' || as->word1[0] == ':') { // String literal or label
			// Get current file offset
			long current_fo = ftell(as->outfile);
			if (current_fo < 0) {
				stasm_msgf(SMT_ERROR, "failed to get offset with error %d", errno);
				return 1;
			}
			// Compute current address
			uint64_t curr_addr = as->curr_sec.addr + current_fo - as->curr_sec_fo;

			// Note usage of label
			struct label_usage *lu = (struct label_usage*)malloc(sizeof(struct label_usage));
			label_usage_init(lu, current_fo + opcode_size, curr_addr + opcode_size);

			// See if label address is known
			uint64_t addr = 0;
			struct label_rec *rec = label_rec_lookup(as->label_recs, true, as->word1);
			if (!rec) {
				// Label has not been used before. Add new label record to list.
				struct label_rec *next = (struct label_rec*)malloc(sizeof(struct label_rec));
				label_rec_init(next, as->word1[0] == '"', false, bstrdupb(as->word1), addr, lu);
				next->prev = as->label_recs;
				as->label_recs = next;
			}
			else if (rec->defined) {
				// The address for this label is known
				// todo: Don't discard here
				free(lu);
				addr = rec->addr;
			}
			else {
				// This label has been used before but does not have an address yet.
				// Note the usage and move on.
				lu->prev = rec->usages;
				rec->usages = lu;
			}

			// @todo: Look up max immediate size

			imm_val = addr;
			imm_bytes_reqd = 8; // @todo
		}
		else { // Integer literal
			assert(as->pret1);

			imm_val = as->pval1;
			imm_bytes_reqd = min_bytes_for_val(imm_val);

			if (as->state == APS_PUSH2) {
				// Compact push pseudo-ops where possible
				int opcode;
				switch (as->code) {
				case PSOP_PUSH16:
					if (imm_bytes_reqd < 2) {
						if (imm_val < 0) opcode = op_push8asi16;
						else opcode = op_push8asu16;
					}
					else {
						opcode = op_push16as16;
					}
					break;
				case PSOP_PUSH32:
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
				case PSOP_PUSH64:
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
				case PSOP_PUSH8:
					// Already as compact as possible
					opcode = op_push8as8;
					break;
				default:
					assert(false);
				}
				buff[0] = opcode;
				as->sdt = imm_type_for_opcode(opcode);
				imm_bytes = sdt_size(as->sdt);
			}
		}

		// Check that value fits into buffer
		if (imm_bytes_reqd > imm_bytes) {
			stasm_msgf(SMT_ERROR | SMF_USETOK, "immediate value \"%s\" is out of bounds for type", as->word1);
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

	/*
	buff[0] = pe.inst.opcode;
	memset(buff + 1, 0, sizeof(buff) - 1);

	// Get the immediate type
	int sdt = imm_type_for_opcode(pe.inst.opcode);
	if (sdt < 0) {
		const char *opname = name_for_opcode(pe.inst.opcode);
		stasm_msgf(SMT_ERROR, "failed to get immediate types for opcode 0x%02x (%s)",
			pe.inst.opcode, opname ? opname : "unknown");
		error = 1;
		break;
	}
	// Get the immediate size
	int imm_bytes = sdt_size(sdt);

	// Initialize potential label usage
	struct label_usage *lu = NULL;
	struct label_rec *rec = NULL;

	if (pe.inst.imm != NULL) { // There is an immediate value
		if (sdt == SDT_VOID || imm_bytes == 0) { // Internal error
			const char *opname = name_for_opcode(pe.inst.opcode);
			stasm_msgf(SMT_ERROR, "unexpected immediate value for opcode 0x%02x (%s)",
				pe.inst.opcode, opname ? opname : "unknown");
			error = 1;
			break;
		}
		if (pe.inst.imm[0] == ':' || pe.inst.imm[0] == '"') { // Label or string literal immediate
			// Get current file offset
			long current_fo = ftell(outfile);
			if (current_fo < 0) {
				stasm_msgf(SMT_ERROR, "failed to get offset with error %d", errno);
				error = 1;
				break;
			}
			// Compute current address
			uint64_t curr_addr = curr_sec.addr + current_fo - curr_sec_fo;

			// Create a usage record for this label
			lu = (struct label_usage*)malloc(sizeof(struct label_usage));
			label_usage_init(lu, current_fo, curr_addr);

			// Look up the label record
			bool string_lit = pe.inst.imm[0] == '"';
			if (string_lit) {
				// Parse and store string literal contents, not representation
				bchar *contents = balloc();
				bool parse_ret = parse_string_lit(pe.inst.imm, &contents);
				if (!parse_ret) { // Internal error, this should have been checked by parser
					bfree(contents);
					stasm_msgf(SMT_ERROR, "failed to parse string literal");
					error = 1;
					break;
				}
				bfree(pe.inst.imm);
				pe.inst.imm = contents;
			}
			rec = get_rec_for_label(label_recs, string_lit, pe.inst.imm);
			if (rec == NULL) {
				// Label has not yet been used or defined. Add new label record to list.
				struct label_rec *next = (struct label_rec*)malloc(sizeof(struct label_rec));
				label_rec_init(next, string_lit, pe.inst.imm, 0, lu);
				lu = NULL;
				pe.inst.imm = NULL;
				next->prev = label_recs;
				label_recs = next;
			}
			else if (rec->usages) {
				// Label has been used before but is not defined. Add label usage to list.
				lu->prev = rec->usages;
				rec->usages = lu;
				lu = NULL;
			}
			// Otherwise the label is already defined
		}
		else { // Numeric literal
			// Parse numeric literal
			int64_t immval = 0;
			bool success = parse_int(pe.inst.imm, &immval);
			if (!success) {
				stasm_msgf(SMT_ERROR, "failed to parse integer literal \"%s\"", pe.inst.imm);
				error = 1;
				break;
			}

			// Check numeric literal immediate bounds
			if (imm_bytes < 8) {
				int64_t minval, maxval;
				sdt_min_max(sdt, &minval, &maxval);
				if (immval < minval || immval > maxval) {
					stasm_msgf(SMT_ERROR, "literal \"%s\" is out of bounds for type", pe.inst.imm);
					error = 1;
					break;
				}
			}

			put_little64((uint64_t)immval, buff + 1);
		}
	}
	else if (sdt != SDT_VOID || imm_bytes != 0) { // Internal error
		const char *opname = name_for_opcode(pe.inst.opcode);
		stasm_msgf(SMT_ERROR, "expected immediate value for opcode 0x%02x (%s)",
			pe.inst.opcode, opname ? opname : "unknown");
		error = 1;
		break;
	}

	// Write instruction to output file
	size_t written = fwrite(buff, 1, imm_bytes + 1, outfile);
	if (written != (size_t)imm_bytes + 1) {
		stasm_msgf(SMT_ERROR, "failed to write to output file %s, errno %d", arg_output, errno);
		if (lu) {
			free(lu);
		}
		error = 1;
		break;
	}

	if (lu) {
		// Label already defined and can be applied right away
		error = apply_label_usage(outfile, lu, rec->addr);
		free(lu);
	}
	*/
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
			if (symbol[1] == '\0' || strchr(symbol + 1, ':') != NULL) {
				// Label name may not contain ':' and must not be empty
				stasm_msgf(SMT_ERROR | SMF_USETOK, "invalid label name");
				ret = 1;
				break;
			}
			ret = assembler_handle_label_def(as, symbol);
			break;
		}

		// Check for assembler command without substitution
		int code = get_asm_cmd(token), sdt = -1;
		switch (code) {
		case ASM_CMD_DATA16:
			sdt = SDT_A16;
			nextstate = APS_DATA1;
			break;
		case ASM_CMD_DATA32:
			sdt = SDT_A32;
			nextstate = APS_DATA1;
			break;
		case ASM_CMD_DATA64:
			sdt = SDT_A64;
			nextstate = APS_DATA1;
			break;
		case ASM_CMD_DATA8:
			sdt = SDT_A8;
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
			code = opcode_for_name(symbol);
			if (code < 0) {
				// Instruction is not an exact match for any opcode. Check for pseudo-ops.
				code = get_psop(symbol);
				switch (code) {
				case PSOP_PUSH16:
					sdt = SDT_A16;
					nextstate = APS_PUSH1;
					break;
				case PSOP_PUSH32:
					sdt = SDT_A32;
					nextstate = APS_PUSH1;
					break;
				case PSOP_PUSH64:
					sdt = SDT_A64;
					nextstate = APS_PUSH1;
					break;
				case PSOP_PUSH8:
					sdt = SDT_A8;
					nextstate = APS_PUSH1;
					break;
				default:
					// @todo: Should put into a state where we ignore tokens until '\n'?
					stasm_msgf(SMT_ERROR | SMF_USETOK, "unrecognized opcode \"%s\"", symbol);
					ret = 1;
					break;
				}
			}
			else {
				sdt = imm_type_for_opcode(code);
				nextstate = sdt == SDT_VOID ? APS_OPCODE2 : APS_OPCODE1;
			}
			break;
		}
		as->code = code;
		as->sdt = sdt;
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
			// word1 will be NULL if the opcode expects no immediate value.
			// Otherwise it will be a quoted string, label, or integer literal.
			// Otherwise an error message has been emitted.
			if (!as->word1 || as->word1[0] == '"' || as->word1[0] == ':' || as->pret1) {
				ret = assembler_handle_opcode(as);
			}
			break;

		case APS_SECTION2:
			// Section address is in word1
			// @todo: Allow section flags
			if (as->pret1) { // Address parsed successfully
				ret = assembler_handle_section(as, (uint64_t)as->pval1);
			}
			break;

		case APS_STRINGS:
			ret = assembler_handle_strings(as);
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
