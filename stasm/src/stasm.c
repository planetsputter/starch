// stasm.c

#include <errno.h>
#include <stdlib.h>

#include "bstr.h"
#include "carg.h"
#include "lits.h"
#include "parser.h"
#include "starch.h"
#include "stub.h"

// Variables set by command-line arguments
const char *arg_help = NULL;
const char *arg_src = NULL;
const char *arg_output = "a.stb";
const char *arg_maxnsec = NULL;
int maxnsec = 4;

struct carg_desc arg_descs[] = {
	{
		CARG_TYPE_UNARY, // Type
		'h',            // Flag
		"--help",       // Name
		&arg_help,      // Value
		false,          // Required
		"show usage",   // Usage text
		NULL            // Value hint
	},
	{
		CARG_TYPE_NAMED, // Type
		'o',             // Flag
		"--output",      // Name
		&arg_output,     // Value
		false,           // Required
		"binary output", // Usage text
		"output"         // Value hint
	},
	{
		CARG_TYPE_NAMED, // Type
		'\0',             // Flag
		"--maxnsec",     // Name
		&arg_maxnsec,    // Value
		false,           // Required
		"maximum number of sections", // Usage text
		"maxnsec"        // Value hint
	},
	{
		CARG_TYPE_POSITIONAL, // Type
		'\0',                // Flag
		NULL,                // Name
		&arg_src,            // Value
		false,               // Required
		"starch source",     // Usage text
		"source"             // Value hint
	},
	{ CARG_TYPE_NONE }
};

bool non_help_arg = false;
void detect_non_help_arg(struct carg_desc *desc, const char *arg)
{
	(void)arg;
	if (desc->value != &arg_help) {
		non_help_arg = true;
	}
}

// Write the 64-bit unsigned value to eight bytes in little-endian order.
// @todo: Move to utiity library.
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

//
// Include file link
//
struct inc_link {
	FILE *file;
	struct parser parser;
	struct inc_link *prev;
};

// Initializes the given link, taking ownership of the given filename B-string.
// Also takes ownership of the given file, closing it when the object is destroyed.
void inc_link_init(struct inc_link *link, char *filename, FILE *file)
{
	link->file = file;
	parser_init(&link->parser, filename);
	link->prev = NULL;
}

// Destroys the given include link
void inc_link_destroy(struct inc_link *link)
{
	parser_destroy(&link->parser);
	fclose(link->file);
}

//
// Records the use of an undefined label
//
struct label_usage {
	long foffset; // Offset of instruction in file
	uint64_t addr; // Address of usage
	struct label_usage *prev;
};

// Initializes the given label usage
static void label_usage_init(struct label_usage *lu, long foffset, uint64_t addr)
{
	lu->foffset = foffset;
	lu->addr = addr;
	lu->prev = NULL;
}

// Applies the given label usage in the given file once the label address is known
static int apply_label_usage(FILE *outfile, struct label_usage *lu, uint64_t label_addr)
{
	// Seek to the instruction
	int ret = fseek(outfile, lu->foffset, SEEK_SET);
	if (ret) {
		fprintf(stderr, "error: failed to seek in output file \"%s\", errno %d\n", arg_output, errno);
		return ret;
	}

	// Read the instruction opcode
	uint8_t buff[9];
	size_t bc = fread(buff, 1, 1, outfile);
	if (bc != 1) {
		fprintf(stderr, "error: failed to read from output file \"%s\", errno %d\n", arg_output, errno);
		return ret;
	}

	// Get the immediate type
	int sdt = imm_type_for_opcode(buff[0]);
	if (sdt < 0) {
		const char *opname = name_for_opcode(buff[0]);
		fprintf(stderr, "error: failed to get immediate type for opcode 0x%02x (%s)\n",
			buff[0], opname ? opname : "unknown");
		return 1;
	}

	// Get the immediate size
	int imm_bytes = sdt_size(sdt);
	if (imm_bytes < 1 || imm_bytes > 8) {
		fprintf(stderr, "error: invalid immediate size %d for data type %d\n", imm_bytes, sdt);
		return 1;
	}

	// Check that the immediate bytes are all zero
	bc = fread(buff + 1, 1, imm_bytes, outfile);
	if (bc != (size_t)imm_bytes) {
		fprintf(stderr, "error: failed to read from output file \"%s\", errno %d\n", arg_output, errno);
		return ret;
	}
	for (int i = 0; i < imm_bytes; i++) {
		if (buff[i + 1] != 0) {
			fprintf(stderr, "error: immediate data not cleared prior to label application\n");
			return ret;
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
			fprintf(stderr, "error: immediate label value out of range for opcode\n");
			return 1;
		}
		u64_to_little8(imm_val, buff + 1);
	}

	// This is not a jump or branch instruction.
	// All other instructions expecting a 64-bit immediate value can take a label immediate.
	else if (imm_bytes == 8) {
		u64_to_little8(label_addr, buff + 1);
	}
	else {
		fprintf(stderr, "error: opcode does not accept an immediate label\n");
		return 1;
	}

	// Seek back to beginning of instruction
	ret = fseek(outfile, -imm_bytes - 1, SEEK_CUR);
	if (ret) {
		fprintf(stderr, "error: failed to seek in output file \"%s\", errno %d\n", arg_output, errno);
		return 1;
	}

	// Write instruction to output file
	bc = fwrite(buff, 1, imm_bytes + 1, outfile);
	if (bc != (size_t)imm_bytes + 1) {
		fprintf(stderr, "error: failed to write to output file %s, errno %d\n", arg_output, errno);
		return 1;
	}

	return 0;
}

//
// Label record
//
struct label_rec {
	char *label; // B-string
	uint64_t addr; // Only relevant if usages is NULL
	// Track usages until label is defined. Label is defined if this is NULL.
	struct label_usage *usages;
	struct label_rec *prev;
};

// Initializes the given label record, taking ownership of the given label B-string
static void label_rec_init(struct label_rec *rec, char *label, uint64_t addr, struct label_usage *usages)
{
	rec->label = label;
	rec->addr = addr;
	rec->usages = usages;
	rec->prev = NULL;
}

// Destroys the given label record
static void label_rec_destroy(struct label_rec *rec)
{
	bstr_free(rec->label);
	rec->label = NULL;
	for (struct label_usage *usage = rec->usages; usage; ) {
		struct label_usage *prev = usage->prev;
		free(usage);
		usage = prev;
	}
	rec->usages = NULL;
}

// Look up the label record for the given label name
static struct label_rec *get_rec_for_label(struct label_rec *rec, const char *label)
{
	for (; rec; rec = rec->prev) {
		if (strcmp(rec->label, label) == 0) {
			return rec;
		}
	}
	return NULL;
}

int main(int argc, const char *argv[])
{
	// Parse command-line arguments
	enum carg_error parse_error = carg_parse_args(
		arg_descs,
		detect_non_help_arg,
		NULL,
		argc,
		argv
	);
	if (arg_help) {
		// Usage requested
		if (non_help_arg) {// Other arguments present
			fprintf(stderr, "warning: Only printing usage. Other arguments present.\n");
		}
		carg_print_usage(argv[0], arg_descs);
		return parse_error != CARG_ERROR_NONE;
	}
	if (parse_error != CARG_ERROR_NONE) {
		// Parse arguments again, printing errors
		parse_error = carg_parse_args(
			arg_descs,
			NULL,
			carg_print_error,
			argc,
			argv
		);
		return 1;
	}
	if (arg_maxnsec) {
		// If a maxnsec argument is given, attempt to parse as an integer
		char *end = NULL;
		maxnsec = strtol(arg_maxnsec, &end, 0);
		if (!end || *end != '\0' || *arg_maxnsec == '\0') {
			fprintf(stderr, "error: failed to parse --maxnsec argument \"%s\"\n", arg_maxnsec);
			return 1;
		}
	}
	// Arguments parsed, no errors

	// Open input file
	FILE *infile;
	if (arg_src) {
		infile = fopen(arg_src, "r");
		if (!infile) {
			fprintf(stderr, "error: failed to open %s\n", arg_src ? arg_src : "stdin");
			return 1;
		}
	}
	else {
		infile = stdin;
	}

	// Open output file
	FILE *outfile = fopen(arg_output, "w+b");
	if (!outfile) {
		if (infile != stdin) {
			fclose(infile);
		}
		fprintf(stderr, "error: failed to open \"%s\" for writing\n", arg_output);
		return 1;
	}

	// Initialize output file
	int error = stub_init(outfile, maxnsec);
	if (error) {
		fclose(outfile);
		if (infile != stdin) {
			fclose(infile);
		}
		fprintf(stderr, "error: failed to initialize output stub file \"%s\"\n", arg_output);
		return 1;
	}

	// Create include chain
	struct inc_link *inc_chain = (struct inc_link*)malloc(sizeof(struct inc_link));
	inc_link_init(inc_chain, arg_src ? bstr_dup(arg_src) : NULL, infile);

	// Initialize label definition list.
	// @todo: Make a binary search tree for efficient lookup.
	struct label_rec *label_recs = NULL;

	// Set up assembler state
	int sec_count = 0; // Count of sections encountered
	long curr_sec_fo = 0; // File offset of current section data
	struct stub_sec curr_sec; // The current section
	stub_sec_init(&curr_sec, 0, 0, 0);

	// Parse all bytes of input files until end of files is reached or an error occurs
	struct parser_event pe;
	for (; error == 0 && inc_chain; ) {
		parser_event_init(&pe);

		// Get a byte from the input file
		int c = fgetc(inc_chain->file);
		if (c == EOF) { // Finish file
			if (ferror(inc_chain->file)) {
				fprintf(stderr, "error: failed to read from \"%s\", errno %d\n",
					inc_chain->parser.filename, errno);
				error = 1;
			}
			else if (!parser_can_terminate(&inc_chain->parser)) {
				fprintf(stderr, "error: unexpected EOF in \"%s\"\n",
					inc_chain->parser.filename);
				error = 1;
			}
			else {
				error = parser_terminate(&inc_chain->parser, &pe);

				// Remove the front link
				if (inc_chain->prev) {
					// Transfer symbol definitions
					smap_move(&inc_chain->parser.defs, &inc_chain->prev->parser.defs);
				}
				struct inc_link *prev = inc_chain->prev;
				inc_link_destroy(inc_chain);
				free(inc_chain);
				inc_chain = prev;
			}
		}
		else { // Parse byte
			error = parser_parse_byte(&inc_chain->parser, c, &pe);
		}
		if (error) break;

		// Handle parser event
		switch (pe.type) {
		case PET_NONE: // No event
			break;

		//
		// Section definition
		//
		case PET_SECTION:
			if (sec_count) { // Save the current section before starting a new one
				error = stub_save_section(outfile, sec_count - 1, &curr_sec);
			}
			if (error == 0) {
				// Start a new section. Section address and flags are known but size is unknown at this point.
				stub_sec_init(&curr_sec, pe.sec.addr, pe.sec.flags, 0);
				error = stub_save_section(outfile, sec_count, &curr_sec);
			}
			if (error) {
				fprintf(stderr, "error: failed to save stub section %d with error %d\n", sec_count, error);
				break;
			}
			error = stub_load_section(outfile, sec_count, &curr_sec);
			if (error) {
				fprintf(stderr, "error: failed to load stub section %d with error %d\n", sec_count, error);
				break;
			}
			curr_sec_fo = ftell(outfile);
			if (curr_sec_fo < 0) {
				fprintf(stderr, "error: failed to get offset of section %d with error %d\n", sec_count, errno);
				error = 1;
			}
			else {
				sec_count++;
			}
			break;

		//
		// Instruction
		//
		case PET_INST: {
			if (sec_count == 0) {
				fprintf(stderr, "error: expected section definition before first instruction\n");
				error = 1;
				break;
			}

			// Prepare bytes to write to output file
			uint8_t buff[9]; // Maximum instruction length is 9
			buff[0] = pe.inst.opcode;
			memset(buff + 1, 0, sizeof(buff) - 1);

			// Get the immediate type
			int sdt = imm_type_for_opcode(pe.inst.opcode);
			if (sdt < 0) {
				const char *opname = name_for_opcode(pe.inst.opcode);
				fprintf(stderr, "error: failed to get immediate types for opcode 0x%02x (%s)\n",
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
					fprintf(stderr, "error: unexpected immediate value for opcode 0x%02x (%s)\n",
						pe.inst.opcode, opname ? opname : "unknown");
					error = 1;
					break;
				}
				if (pe.inst.imm[0] == '"') { // Quoted string immediate
					// @todo: Support for operations that accept 64-bit values
					const char *opname = name_for_opcode(pe.inst.opcode);
					fprintf(stderr, "error: string literals not supported for opcode 0x%02x (%s)\n",
						pe.inst.opcode, opname ? opname : "unknown");
					error = 1;
					break;
				}
				if (pe.inst.imm[0] == ':') { // Label immediate
					// Get current file offset
					long current_fo = ftell(outfile);
					if (current_fo < 0) {
						fprintf(stderr, "error: failed to get offset with error %d\n", errno);
						error = 1;
						break;
					}
					// Compute current address
					uint64_t curr_addr = curr_sec.addr + current_fo - curr_sec_fo;

					// Create a usage record for this label
					lu = (struct label_usage*)malloc(sizeof(struct label_usage));
					label_usage_init(lu, current_fo, curr_addr);

					// Look up the label record
					rec = get_rec_for_label(label_recs, pe.inst.imm);
					if (rec == NULL) {
						// Label has not yet been used or defined. Add new label record to list.
						struct label_rec *next = (struct label_rec*)malloc(sizeof(struct label_rec));
						label_rec_init(next, pe.inst.imm, 0, lu);
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
						fprintf(stderr, "error: failed to parse integer literal \"%s\" in \"%s\" line %d char %d\n",
							pe.inst.imm, inc_chain->parser.filename, inc_chain->parser.tline, inc_chain->parser.tch);
						error = 1;
						break;
					}

					// Check numeric literal immediate bounds
					if (imm_bytes < 8) {
						int64_t minval, maxval;
						sdt_min_max(sdt, &minval, &maxval);
						if (immval < minval || immval > maxval) {
							fprintf(stderr, "error: literal \"%s\" is out of bounds for type in \"%s\" line %d char %d\n",
									pe.inst.imm, inc_chain->parser.filename, inc_chain->parser.tline, inc_chain->parser.tch);
							error = 1;
							break;
						}
					}

					u64_to_little8((uint64_t)immval, buff + 1);
				}

			}
			else if (sdt != SDT_VOID || imm_bytes != 0) { // Internal error
				const char *opname = name_for_opcode(pe.inst.opcode);
				fprintf(stderr, "error: expected immediate value for opcode 0x%02x (%s)\n",
					pe.inst.opcode, opname ? opname : "unknown");
				error = 1;
				break;
			}

			// Write instruction to output file
			size_t written = fwrite(buff, 1, imm_bytes + 1, outfile);
			if (written != (size_t)imm_bytes + 1) {
				fprintf(stderr, "error: failed to write to output file %s, errno %d\n", arg_output, errno);
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
		}	break;

		//
		// Raw data
		//
		case PET_DATA: {
			if (sec_count == 0) {
				fprintf(stderr, "error: expected section definition before raw data\n");
				error = 1;
				break;
			}

			// Write raw data to output file
			size_t written = fwrite(pe.data.raw, 1, pe.data.len, outfile);
			if (written != (size_t)pe.data.len) {
				fprintf(stderr, "error: failed to write to output file %s, errno %d\n", arg_output, errno);
				error = 1;
				break;
			}
		}	break;

		//
		// Include file
		//
		case PET_INCLUDE:
			// @todo: include paths
			// Attempt to open the included file
			infile = fopen(pe.filename, "r");
			if (!infile) { // Failed to open included file
				fprintf(stderr, "error: failed to open \"%s\" included from \"%s\" line %d\n",
					pe.filename, inc_chain->parser.filename,
					inc_chain->parser.tline);
				error = 1;
			}
			else { // File opened successfully
				// Add link to front of chain
				struct inc_link *next = (struct inc_link*)malloc(sizeof(struct inc_link));
				inc_link_init(next, pe.filename, infile);
				pe.filename = NULL; // To prevent destruction of B-string
				// Transfer symbol definitions
				smap_move(&inc_chain->parser.defs, &next->parser.defs);
				next->prev = inc_chain;
				inc_chain = next;
			}
			break;

		//
		// Label
		//
		case PET_LABEL: {
			if (sec_count == 0) {
				fprintf(stderr, "error: expected section definition before label\n");
				error = 1;
				break;
			}

			// Compute current position within section
			long fpos = ftell(outfile);
			if (fpos < 0) {
				fprintf(stderr, "error: failed to get offset of label %s with error %d\n", pe.label, error);
				error = 1;
				break;
			}
			// Compute label address
			uint64_t addr = fpos - curr_sec_fo + curr_sec.addr;

			// Look up any existing label record
			struct label_rec *rec = get_rec_for_label(label_recs, pe.label);
			if (!rec) {
				// Label has not been used before. Add new label record to list.
				struct label_rec *next = (struct label_rec*)malloc(sizeof(struct label_rec));
				label_rec_init(next, pe.label, addr, NULL);
				pe.label = NULL;
				next->prev = label_recs;
				label_recs = next;
			}
			else if (!rec->usages) {
				// A definition already exists for this label
				fprintf(stderr, "error: definition already exists for label %s\n", pe.label);
				error = 1;
			}
			else {
				// Label has been used but not defined. This is the definition.
				rec->addr = addr; // Address is now known
				struct label_usage *lu;
				for (lu = rec->usages; lu; ) {
					// Apply each of the usages
					error = apply_label_usage(outfile, lu, rec->addr);
					if (error) {
						break;
					}

					// Delete and move on to the next usage
					struct label_usage *prev = lu->prev;
					free(lu);
					lu = prev;
				}
				rec->usages = lu;

				// Seek back to the original position
				error = fseek(outfile, fpos, SEEK_SET);
				if (error) {
					fprintf(stderr, "error: failed to seek in output file \"%s\", errno %d\n", arg_output, errno);
				}
			}
		}	break;

		default: // Invalid event type
			fprintf(stderr, "error: invalid parser event type %d\n", pe.type);
			error = 1;
			break;
		}

		parser_event_destroy(&pe); // Destroy the parser event
	}

	if (sec_count) {
		int save_error = stub_save_section(outfile, sec_count - 1, &curr_sec);
		if (save_error) {
			fprintf(stderr, "error: failed to save section %d to \"%s\"\n", sec_count - 1, arg_output);
			if (error == 0) {
				error = save_error;
			}
		}
	}

	// Destroy label record list
	bool undefined_label = false;
	for (; label_recs;) {
		// Check for undefined labels.
		// It's not worth printing these errors if another error already occurred, because
		// the other error may have halted assembly before the labels were defined.
		if (error == 0 && label_recs->usages) {
			fprintf(stderr, "error: use of undefined label \"%s\"\n", label_recs->label);
			undefined_label = true;
		}
		struct label_rec *prev = label_recs->prev;
		label_rec_destroy(label_recs);
		free(label_recs);
		label_recs = prev;
	}
	if (undefined_label) {
		error = 1;
	}

	// Destroy include chain
	for (; inc_chain; ) {
		struct inc_link *prev = inc_chain->prev;
		inc_link_destroy(inc_chain);
		free(inc_chain);
		inc_chain = prev;
	}

	// Close output file
	fclose(outfile);

	return error;
}
