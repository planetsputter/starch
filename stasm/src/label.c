// label.c

#include <errno.h>
#include <stdlib.h>

#include "label.h"
#include "starch.h"
#include "stasm.h"
#include "util.h"

// Initializes the given label usage
void label_usage_init(struct label_usage *lu, long foffset, uint64_t addr)
{
	lu->foffset = foffset;
	lu->addr = addr;
	lu->prev = NULL;
}

// Applies the given label usage in the given file once the label address is known
int label_usage_apply(const struct label_usage *lu, FILE *outfile, uint64_t label_addr)
{
	// Seek to the instruction
	int ret = fseek(outfile, lu->foffset, SEEK_SET);
	if (ret) {
		stasm_msgf(SMT_ERROR, "failed to seek in output file, errno %d", errno);
		return ret;
	}

	// Read the instruction opcode
	uint8_t buff[9];
	size_t bc = fread(buff, 1, 1, outfile);
	if (bc != 1) {
		stasm_msgf(SMT_ERROR, "failed to read from output file, errno %d", errno);
		return ret;
	}

	// Get the immediate type
	int sdt = imm_type_for_opcode(buff[0]);
	if (sdt < 0) {
		const char *opname = name_for_opcode(buff[0]);
		stasm_msgf(SMT_ERROR, "failed to get immediate type for opcode 0x%02x (%s)",
			buff[0], opname ? opname : "unknown");
		return 1;
	}

	// Get the immediate size
	int imm_bytes = sdt_size(sdt);
	if (imm_bytes < 1 || imm_bytes > 8) {
		stasm_msgf(SMT_ERROR, "invalid immediate size %d for data type %d", imm_bytes, sdt);
		return 1;
	}

	// Check that the immediate bytes are all zero
	bc = fread(buff + 1, 1, imm_bytes, outfile);
	if (bc != (size_t)imm_bytes) {
		stasm_msgf(SMT_ERROR, "failed to read from output file, errno %d", errno);
		return 1;
	}
	for (int i = 0; i < imm_bytes; i++) {
		if (buff[i + 1] != 0) {
			stasm_msgf(SMT_ERROR, "immediate data not cleared prior to label application");
			return 1;
		}
	}

	// Check for jump/branch opcodes that explicitly accept and interpret a label immediate
	int jmp_br = 0, use_delta = 0;
	opcode_is_jmp_br(buff[0], &jmp_br, &use_delta);
	if (jmp_br) {
		int64_t imm_val = use_delta ? label_addr - lu->addr : label_addr;
		// Bounds-check
		int64_t min_val = 0, max_val = 0;
		sdt_min_max(sdt, &min_val, &max_val);
		if (imm_bytes < 8 && (imm_val < min_val || imm_val > max_val)) {
			stasm_msgf(SMT_ERROR, "immediate label value out of range for opcode");
			return 1;
		}
		put_little64(imm_val, buff + 1);
	}

	// This is not a jump or branch instruction.
	// All other instructions expecting a 64-bit immediate value can take a label immediate.
	else if (imm_bytes == 8) {
		put_little64(label_addr, buff + 1);
	}
	else {
		stasm_msgf(SMT_ERROR, "opcode does not accept an immediate label");
		return 1;
	}

	// Seek back to beginning of instruction
	ret = fseek(outfile, -imm_bytes - 1, SEEK_CUR);
	if (ret) {
		stasm_msgf(SMT_ERROR, "failed to seek in output file, errno %d", errno);
		return 1;
	}

	// Write instruction to output file
	bc = fwrite(buff, 1, imm_bytes + 1, outfile);
	if (bc != (size_t)imm_bytes + 1) {
		stasm_msgf(SMT_ERROR, "failed to write to output file, errno %d", errno);
		return 1;
	}

	return 0;
}

// Initializes the given label record, taking ownership of the given label B-string
void label_rec_init(struct label_rec *rec, bool string_lit, bool defined, bchar *label, uint64_t addr, struct label_usage *usages)
{
	rec->string_lit = string_lit;
	rec->label = label;
	rec->defined = defined;
	rec->addr = addr;
	rec->usages = usages;
	rec->prev = NULL;
}

// Destroys the given label record
void label_rec_destroy(struct label_rec *rec)
{
	bfree(rec->label);
	rec->label = NULL;
	for (struct label_usage *usage = rec->usages; usage; ) {
		struct label_usage *prev = usage->prev;
		free(usage);
		usage = prev;
	}
	rec->usages = NULL;
}

// Look up the label record for the given B-string label name or string literal
struct label_rec *label_rec_lookup(struct label_rec *rec, bool string_lit, const bchar *label)
{
	// @todo: Make a binary search tree
	for (; rec; rec = rec->prev) {
		if (rec->string_lit == string_lit && bstrcmpb(rec->label, label) == 0) {
			return rec;
		}
	}
	return NULL;
}
