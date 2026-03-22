// label.c

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

#include "assembler.h"
#include "label.h"
#include "starch.h"
#include "stasm.h"
#include "util.h"

// Initializes the given label usage
void label_usage_init(struct label_usage *lu, long foffset, uint64_t addr, int data_len, bool pseudo_op)
{
	lu->foffset = foffset;
	lu->addr = addr;
	lu->data_len = data_len;
	lu->pseudo_op = pseudo_op;
	lu->prev = NULL;
}

// Applies the given label usage in the given file once the label address is known
int label_usage_apply(const struct label_usage *lu, FILE *outfile, uint64_t label_addr, struct label_rec *recs)
{
	// Record original file position
	long original_fpos = ftell(outfile);
	if (original_fpos < 0) {
		stasm_msgf(SMT_ERROR, "failed to get position in output file, errno %d", errno);
		return 1;
	}

	// Seek to the instruction
	int ret = fseek(outfile, lu->foffset, SEEK_SET);
	if (ret) {
		stasm_msgf(SMT_ERROR, "failed to seek in output file, errno %d", errno);
		return ret;
	}

	// Maximum instruction length is currently 9 bytes
	uint8_t buff[9];
	int64_t imm_val;
	int opcode;

	// Determine the opcode length and immediate value length
	int opcode_len, imm_len, imm_len_reqd;
	if (lu->data_len) {
		// For raw data label usages, there is no opcode and the label value is the immediate
		opcode = 0;
		opcode_len = 0;
		imm_len = lu->data_len;
		imm_len_reqd = imm_len; // Do not compact data
		imm_val = (int64_t)label_addr;
	}
	else {
		// @todo: For now we assume all opcodes are a single byte
		opcode_len = 1;

		// Read the instruction opcode
		size_t bc = fread(buff, 1, 1, outfile);
		if (bc != 1) {
			stasm_msgf(SMT_ERROR, "failed to read from output file, errno %d", errno);
			return 1;
		}
		opcode = buff[0];

		// Get the immediate type
		int sdt = imm_type_for_opcode(opcode);
		if (sdt < 0) {
			const char *opname = name_for_opcode(opcode);
			stasm_msgf(SMT_ERROR, "failed to get immediate type for opcode 0x%02x (%s)",
				opcode, opname ? opname : "unknown");
			return 1;
		}

		// Get the immediate size
		imm_len = sdt_size(sdt);
		if (imm_len < 1 || imm_len > 8) {
			stasm_msgf(SMT_ERROR, "invalid immediate size %d for data type %d", imm_len, sdt);
			return 1;
		}

		// Check for jump/branch opcodes that explicitly accept and interpret a label immediate
		int jmp_br = 0, use_delta = 0;
		opcode_is_jmp_br(opcode, &jmp_br, &use_delta);
		imm_val = use_delta ? label_addr - lu->addr : label_addr;

		if (lu->pseudo_op) {
			// If the label usage was a pseudo-op, compact it if possible
			bool oob = false;
			opcode = assembler_compact_op(opcode, false, imm_val, &oob);
			if (oob) {
				stasm_msgf(SMT_ERROR, "immediate value out of range for opcode");
				return 1;
			}
			if (opcode < 0) {
				stasm_msgf(SMT_ERROR, "failed to compact opcode");
				return 1;
			}
			sdt = imm_type_for_opcode(opcode);
			imm_len_reqd = sdt_size(sdt);
		}
		else { // Do no compact non-pseudo-ops
			imm_len_reqd = imm_len;
		}

		// Seek back to beginning of opcode
		ret = fseek(outfile, -opcode_len, SEEK_CUR);
		if (ret) {
			stasm_msgf(SMT_ERROR, "failed to seek in output file, errno %d", errno);
			return 1;
		}
	}

	// Bounds-check immediate value
	if (imm_len_reqd < min_bytes_for_val(imm_val)) {
		stasm_msgf(SMT_ERROR, "immediate label value out of range for opcode");
		return 1;
	}
	assert(imm_len_reqd <= imm_len);

	if (opcode_len > 0) {
		// Put (potentially adjusted) opcode into buffer
		// @todo: For now we require all opcodes to be one byte
		buff[0] = opcode;
	}
	// Put immediate value into buffer
	put_little64(imm_val, buff + opcode_len);

	// Write opcode and immediate value to output file
	size_t bc = fwrite(buff, 1, opcode_len + imm_len_reqd, outfile);
	if (bc != (size_t)opcode_len + imm_len_reqd) {
		stasm_msgf(SMT_ERROR, "failed to write to output file, errno %d", errno);
		return 1;
	}

	int compact_count = imm_len - imm_len_reqd;
	if (compact_count > 0) {
		// Move file contents back, rewriting labels, to compact op
		long begin_compact_fpos = lu->foffset + opcode_len + imm_len_reqd;

		// Move the entire rest of the file contents back by compact_count bytes
		enum { MAX_READ = 256 };
		char read_buf[MAX_READ];
		for (;;) {
			// Skip the bytes being compacted
			ret = fseek(outfile, compact_count, SEEK_CUR);
			if (ret) {
				stasm_msgf(SMT_ERROR, "failed to seek in output file, errno %d", errno);
				return 1;
			}

			// Read a block
			bc = fread(read_buf, 1, MAX_READ, outfile);
			if (ferror(outfile)) {
				stasm_msgf(SMT_ERROR, "failed to read from output file, errno %d", errno);
				return 1;
			}
			if (bc == 0) { // End of file reached
				break;
			}

			// Seek back to the beginning of the compacted bytes
			ret = fseek(outfile, -bc - compact_count, SEEK_CUR);
			if (ret) {
				stasm_msgf(SMT_ERROR, "failed to seek in output file, errno %d", errno);
				return 1;
			}

			// Re-write the block
			bc = fwrite(read_buf, 1, bc, outfile);
			if (ferror(outfile)) {
				stasm_msgf(SMT_ERROR, "failed to write to output file, errno %d", errno);
				return 1;
			}
		}

		// We are now at EOF. Move back.
		ret = fseek(outfile, -compact_count, SEEK_CUR);
		if (ret) {
			stasm_msgf(SMT_ERROR, "failed to seek in output file, errno %d", errno);
			return 1;
		}

		// Get file position
		long fpos = ftell(outfile);
		if (fpos < 0) {
			stasm_msgf(SMT_ERROR, "failed to get position in output file, errno %d", errno);
			return 1;
		}

		// Truncate excess content
		ret = fflush(outfile);
		if (ret) {
			stasm_msgf(SMT_ERROR, "failed to flush output file, errno %d", errno);
			return 1;
		}
		ret = ftruncate(fileno(outfile), fpos);
		if (ret) {
			stasm_msgf(SMT_ERROR, "failed to truncate output file, errno %d", errno);
			return 1;
		}

		// Get the number of stub sections
		int maxnsec = 0, nsec = 0;
		ret = stub_get_section_counts(outfile, &maxnsec, &nsec);
		if (ret || nsec < 1) {
			stasm_msgf(SMT_ERROR, "failed to get stub section counts, stub error %d", ret);
			return 1;
		}

		// Update all stub section headers, noting which section contains the label usage
		long end_compact_section = 0;
		struct stub_sec sec;
		for (int si = 0; si < nsec; si++) {
			// Load the section header information and seek to beginning of section data
			ret = stub_load_section(outfile, si, &sec);
			if (ret) {
				stasm_msgf(SMT_ERROR, "failed to load stub section, stub error %d", ret);
				return 1;
			}

			// Seek to end of section data. The last section will not have a size yet
			// but will continue to the end of the file.
			if (si == nsec - 1) {
				ret = fseek(outfile, 0, SEEK_END);
			}
			else {
				ret = fseek(outfile, sec.size, SEEK_CUR);
			}
			if (ret) {
				stasm_msgf(SMT_ERROR, "failed to seek in output file, errno %d", errno);
				return 1;
			}

			// Get file position
			fpos = ftell(outfile);
			if (fpos < 0) {
				stasm_msgf(SMT_ERROR, "failed to get position in output file, errno %d", errno);
				return 1;
			}

			// Adjust file position if end of section has been moved
			if (fpos >= begin_compact_fpos) {
				if (end_compact_section == 0) {
					end_compact_section = fpos;
					if (si < nsec - 1) {
						end_compact_section -= compact_count;
					}
				}

				// Do not save last section, which is in progress
				if (si < nsec - 1) {
					ret = fseek(outfile, -compact_count, SEEK_CUR);
					if (ret) {
						stasm_msgf(SMT_ERROR, "failed to seek in output file, errno %d", errno);
						return 1;
					}

					// Re-write section header
					ret = stub_save_section(outfile, si, &sec);
					if (ret) {
						stasm_msgf(SMT_ERROR, "failed to save stub section, stub error %d", ret);
						return 1;
					}
				}
			}
		}

		if (end_compact_section == 0) {
			stasm_msgf(SMT_ERROR, "failed to identify stub section for label usage");
			return 1;
		}

		// Adjust all label record and usage file positions.
		// Note that this will also adjust the file position of *lu passed to this function.
		// @todo: It would be more correct to save a section index for each label instead of a file position.
		// Labels defined at the beginning or end of a section have a file position which is ambiguous.
		for (struct label_rec *rec = recs; rec; rec = rec->prev) {
			if (rec->defined && rec->fpos > begin_compact_fpos) {
				// Adjust file positions of labels defined later in the file
				rec->fpos -= compact_count;
				if (rec->fpos <= end_compact_section) {
					// Adjust addresses of labels defined later in the same section as the compacted usage
					rec->addr -= compact_count;
				}
			}
			for (struct label_usage *l = rec->usages; l; l = l->prev) {
				if (l->foffset > begin_compact_fpos) {
					// Adjust file positions of label usages later in the file
					l->foffset -= compact_count;
					if (l->foffset <= end_compact_section) {
						// Adjust addresses of label usages later in the same section as the compacted usage
						l->addr -= compact_count;
					}
				}
			}
		}

		// Now that all labels and usages are updated, re-apply each one.
		// This is definitely not the most efficient approach, but it is the easiest way to be correct.
		for (struct label_rec *rec = recs; rec; rec = rec->prev) {
			if (!rec->defined) continue;
			for (struct label_usage *l = rec->usages; l; l = l->prev) {
				ret = label_usage_apply(l, outfile, rec->addr, recs);
				if (ret) { // Error will already have been logged
					return ret;
				}
			}
		}
	}

	// Seek back to the original file position, allowing for compaction
	ret = fseek(outfile, original_fpos - compact_count, SEEK_SET);
	if (ret) {
		stasm_msgf(SMT_ERROR, "failed to seek in output file, errno %d", errno);
		return 1;
	}

	return 0;
}

// Initializes the given label record, taking ownership of the given label B-string
void label_rec_init(struct label_rec *rec, bool string_lit, bool defined, bchar *label, uint64_t addr, long fpos, struct label_usage *usages)
{
	rec->string_lit = string_lit;
	rec->label = label;
	rec->defined = defined;
	rec->addr = addr;
	rec->fpos = fpos;
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
