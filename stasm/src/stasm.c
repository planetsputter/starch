// stasm.c

#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>

#include "assembler.h"
#include "bstr.h"
#include "carg.h"
#include "lits.h"
#include "starch.h"
#include "stasm.h"
#include "stub.h"
#include "tokenizer.h"
#include "utf8.h"
#include "util.h"

// Variables set by command-line arguments
static const char *arg_help = NULL;
static const char *arg_src = NULL;
static const char *arg_output = "a.stb";
static const char *arg_maxnsec = NULL;
static int maxnsec = 4;

static struct carg_desc arg_descs[] = {
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

static bool non_help_arg = false;
static void detect_non_help_arg(struct carg_desc *desc, const char *arg)
{
	(void)arg;
	if (desc->value != &arg_help) {
		non_help_arg = true;
	}
}

// Token line and character number
static int tlineno, tcharno;

//
// Include file link
//
struct inc_link {
	bchar *filename;
	FILE *file;
	int lineno, charno;
	struct inc_link *prev;
};

// The include chain
static struct inc_link *inc_chain = NULL;

// Initializes the given link, taking ownership of the given filename B-string.
// Also takes ownership of the given file, closing it when the object is destroyed.
static void inc_link_init(struct inc_link *link, bchar *filename, FILE *file)
{
	link->filename = filename;
	link->file = file;
	link->lineno = 1;
	link->charno = 0; // Will be incremented before processing first character
	link->prev = NULL;
}

// Destroys the given include link
static void inc_link_destroy(struct inc_link *link)
{
	if (link->filename) {
		bfree(link->filename);
		link->filename = NULL;
	}
	if (link->file) {
		if (link->file != stdin) {
			fclose(link->file);
		}
		link->file = NULL;
	}
}

// Stasm message counts by type
static size_t msg_counts[SMT_NUM];
size_t stasm_count_msg(int msg_type)
{
	return msg_counts[msg_type];
}

// Message type names including ANSI color codes
#define ANSI_YELLOW "\033[33m"
#define ANSI_RED "\033[31m"
#define ANSI_NORMAL "\033[0m"
static const char *stasm_msg_types[] = {
	"info",
	ANSI_YELLOW "warning" ANSI_NORMAL,
	ANSI_RED "error" ANSI_NORMAL,
};

int stasm_msgf(int msg_type, const char *format, ...)
{
	// Determine whether to use current or token line and character numbers
	bool usetoknums = msg_type & SMF_USETOK;
	msg_type &= ~SMF_USETOK;

	// Prepend message type name
	FILE *out_file = msg_type <= SMT_INFO ? stdout : stderr;
	fprintf(out_file, "%s: ", stasm_msg_types[msg_type]);

	// Prepend the current file and line if it is known
	struct inc_link *link = inc_chain;
	if (link) {
		int line = usetoknums ? tlineno : link->lineno;
		int ch = usetoknums ? tcharno : link->charno;
		fprintf(out_file, "%s:%d:%d: ", link->filename, line, ch);
	}

	// Format and print the given message
	va_list args;
	va_start(args, format);
	int ret = vfprintf(out_file, format, args);
	va_end(args);
	fprintf(out_file, "\n"); // Append newline

	// Print the include chain
	if (link) {
		link = link->prev;
	}
	for (; link; link = link->prev) {
		fprintf(out_file, "  in file included from %s:%d\n", link->filename, link->lineno - 1);
	}

	msg_counts[msg_type]++;
	return ret;
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
			stasm_msgf(SMT_ERROR, "warning: Only printing usage. Other arguments present.");
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
			stasm_msgf(SMT_ERROR, "failed to parse --maxnsec argument \"%s\"", arg_maxnsec);
			return 1;
		}
	}
	// Arguments parsed, no errors

	// Open input file
	FILE *infile;
	if (arg_src) {
		infile = fopen(arg_src, "r");
		if (!infile) {
			stasm_msgf(SMT_ERROR, "failed to open %s", arg_src ? arg_src : "stdin");
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
		stasm_msgf(SMT_ERROR, "failed to open \"%s\" for writing", arg_output);
		return 1;
	}

	// Initialize output file
	int ret = stub_init(outfile, maxnsec);
	if (ret) {
		fclose(outfile);
		if (infile != stdin) {
			fclose(infile);
		}
		stasm_msgf(SMT_ERROR, "failed to initialize output stub file \"%s\"", arg_output);
		return 1;
	}

	// Initialize include chain
	inc_chain = (struct inc_link*)malloc(sizeof(struct inc_link));
	inc_link_init(inc_chain, arg_src ? bstrdupc(arg_src) : NULL, infile);

	// Initialize decoder
	struct utf8_decoder decoder;
	utf8_decoder_init(&decoder);

	// Initialize tokenizer
	struct tokenizer tokenizer;
	tokenizer_init(&tokenizer);

	// Initialize assembler
	struct assembler as;
	assembler_init(&as, outfile);

	bool pop_inc = false;
	ucp c = 0;
	bool tip = false; // Token in progress

	// Parse each byte of the source file and each included file
	while (inc_chain) {
		// Check whether a token is in progress
		if (!tip && tokenizer_in_progress(&tokenizer)) {
			tip = true;
			tlineno = inc_chain->lineno;
			tcharno = inc_chain->charno;
		}

		// Assemble any emitted tokens
		bchar *token = NULL;
		tokenizer_emit(&tokenizer, &token);
		if (token) {
			tip = false;
			// Assemble the token
			ret = assembler_handle_token(&as, token);
			if (ret) break;

			// Check for included file
			bchar *filename = NULL;
			assembler_get_include(&as, &filename);
			if (filename) {
				// Open included file
				FILE *incfile = fopen(filename, "r");
				if (!incfile) {
					stasm_msgf(SMT_ERROR, "failed to open %s", filename);
					ret = 1;
					break;
				}

				// Add link to chain
				struct inc_link *link = (struct inc_link*)malloc(sizeof(struct inc_link));
				inc_link_init(link, filename, incfile);
				link->prev = inc_chain;
				inc_chain = link;
			}
			continue;
		}

		// Pop the included file if requested
		if (pop_inc) {
			pop_inc = false;

			// Remove the front link
			struct inc_link *prev = inc_chain->prev;
			inc_link_destroy(inc_chain);
			free(inc_chain);
			inc_chain = prev;
			if (!inc_chain) break; // Assembled all files
		}
		// Keep track of line and character numbers
		else if (c == '\n') {
			inc_chain->lineno++;
			inc_chain->charno = 1;
		}
		else {
			inc_chain->charno++;
		}

		// Get a character from the current input file
		for (;;) {
			int b = fgetc(inc_chain->file);
			if (b == EOF) { // Finish file
				if (ferror(inc_chain->file)) {// Check for file read error
					stasm_msgf(SMT_ERROR, "read failed, errno %d", errno);
					ret = 1;
					break;
				}
				// Ensure the decoder and tokenizer can terminate now
				if (!utf8_decoder_can_terminate(&decoder) || !tokenizer_finish(&tokenizer)) {
					stasm_msgf(SMT_ERROR, "unexpected EOF");
					ret = 1;
					break;
				}
				// Handle end of file like end of statement
				c = '\n';
				pop_inc = true;
				break;
			}

			// Decode byte
			ucp *cp = utf8_decoder_decode(&decoder, b, &c, &ret);
			if (ret) { // Encoding error
				stasm_msgf(SMT_ERROR, "encoding error");
				break;
			}
			if (cp != &c) break; // Character generated
		}
		if (ret) break;

		// Feed the generated character to the tokenizer
		tokenizer_parse(&tokenizer, c);
	}

	/*
	if (sec_count) {
		// Save the last section of the stub file
		int save_error = stub_save_section(outfile, sec_count - 1, &curr_sec);
		if (save_error) {
			stasm_msgf(SMT_ERROR, "failed to save section %d to \"%s\"", sec_count - 1, arg_output);
			if (ret == 0) {
				ret = save_error;
			}
		}
	}
	*/

	// Destroy tokenizer
	tokenizer_destroy(&tokenizer);

	// Destroy assembler
	assembler_destroy(&as);

	// Destroy include chain
	while (inc_chain) {
		struct inc_link *prev = inc_chain->prev;
		inc_link_destroy(inc_chain);
		free(inc_chain);
		inc_chain = prev;
	}

	// Close output file
	fclose(outfile);

	return ret;

	//
	//
	// OLD CODE
	//
	//

	/*
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
				stasm_msgf(SMT_ERROR, "read failed, errno %d", errno);
				error = 1;
			}
			else if (!parser_can_terminate(&inc_chain->parser)) {
				stasm_msgf(SMT_ERROR, "unexpected EOF");
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
				stasm_msgf(SMT_ERROR, "failed to save stub section %d with error %d", sec_count, error);
				break;
			}
			error = stub_load_section(outfile, sec_count, &curr_sec);
			if (error) {
				stasm_msgf(SMT_ERROR, "failed to load stub section %d with error %d", sec_count, error);
				break;
			}
			curr_sec_fo = ftell(outfile);
			if (curr_sec_fo < 0) {
				stasm_msgf(SMT_ERROR, "failed to get offset of section %d with error %d", sec_count, errno);
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
				stasm_msgf(SMT_ERROR, "expected section definition before first instruction");
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
		}	break;

		//
		// Raw data
		//
		case PET_DATA: {
			if (sec_count == 0) {
				stasm_msgf(SMT_ERROR, "expected section definition before raw data");
				error = 1;
				break;
			}

			// Write raw data to output file
			size_t written = fwrite(pe.data.raw, 1, pe.data.len, outfile);
			if (written != (size_t)pe.data.len) {
				stasm_msgf(SMT_ERROR, "failed to write to output file %s, errno %d", arg_output, errno);
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
				stasm_msgf(SMT_ERROR, "failed to open \"%s\"", pe.filename);
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
				stasm_msgf(SMT_ERROR, "expected section definition before label");
				error = 1;
				break;
			}

			// Compute current position within section
			long fpos = ftell(outfile);
			if (fpos < 0) {
				stasm_msgf(SMT_ERROR, "failed to get offset of label %s with error %d", pe.label, error);
				error = 1;
				break;
			}
			// Compute label address
			uint64_t addr = fpos - curr_sec_fo + curr_sec.addr;

			// Look up any existing label record
			struct label_rec *rec = get_rec_for_label(label_recs, false, pe.label);
			if (!rec) {
				// Label has not been used before. Add new label record to list.
				struct label_rec *next = (struct label_rec*)malloc(sizeof(struct label_rec));
				label_rec_init(next, false, pe.label, addr, NULL);
				pe.label = NULL;
				next->prev = label_recs;
				label_recs = next;
			}
			else if (!rec->usages) {
				// A definition already exists for this label
				stasm_msgf(SMT_ERROR, "definition already exists for label %s", pe.label);
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
				int seek_error = fseek(outfile, fpos, SEEK_SET);
				if (seek_error) {
					stasm_msgf(SMT_ERROR, "failed to seek in output file \"%s\", errno %d", arg_output, errno);
					if (error == 0) error = seek_error;
				}
			}
		}	break;

		//
		// Emit strings
		//
		case PET_STRINGS:
			if (sec_count == 0) {
				stasm_msgf(SMT_ERROR, "expected section definition before .strings");
				error = 1;
				break;
			}

			// Emit all string literal data at the current position
			struct label_rec **pnext = &label_recs;
			for (struct label_rec *rec = label_recs; rec && error == 0;) {
				if (!rec->string_lit) { // This is not a string literal, skip it
					pnext = &rec->prev;
					rec = rec->prev;
					continue;
				}

				if (!rec->usages) { // Internal error
					stasm_msgf(SMT_ERROR, "string literal without usage");
					error = 1;
					break;
				}

				// Note current file offset
				long fpos = ftell(outfile);
				if (fpos < 0) {
					stasm_msgf(SMT_ERROR, "failed to seek in output file \"%s\", errno %d", arg_output, errno);
					error = 1;
					break;
				}
				// Compute string content address
				uint64_t addr = fpos - curr_sec_fo + curr_sec.addr;

				// Write the string literal contents to the file
				size_t write_len = bstrlen(rec->label) + 1;
				size_t bc = fwrite(rec->label, 1, write_len, outfile);
				if (bc != write_len) {
					stasm_msgf(SMT_ERROR, "failed to write to output file, errno %d", errno);
					error = 1;
					break;
				}

				// Apply all usages of the string literal
				rec->addr = addr; // Address is now known
				struct label_usage *lu;
				for (lu = rec->usages; lu;) {
					error = apply_label_usage(outfile, lu, addr);
					if (error) break;

					// Delete and move on to the next usage
					struct label_usage *prev = lu->prev;
					free(lu);
					lu = prev;
				}
				rec->usages = lu;

				// Seek to after the string contents
				int seek_error = fseek(outfile, fpos + write_len, SEEK_SET);
				if (seek_error) {
					stasm_msgf(SMT_ERROR, "failed to seek in output file \"%s\", errno %d", arg_output, errno);
					if (error == 0) error = seek_error;
				}

				// Remove the string literal record from the list of label records
				*pnext = rec->prev;
				label_rec_destroy(rec);
				free(rec);
				rec = *pnext;
			}

			break;

		default: // Invalid event type
			stasm_msgf(SMT_ERROR, "invalid parser event type %d", pe.type);
			error = 1;
			break;
		}

		parser_event_destroy(&pe); // Destroy the parser event
	}

	if (sec_count) {
		// Save the last section of the stub file
		int save_error = stub_save_section(outfile, sec_count - 1, &curr_sec);
		if (save_error) {
			stasm_msgf(SMT_ERROR, "failed to save section %d to \"%s\"", sec_count - 1, arg_output);
			if (error == 0) {
				error = save_error;
			}
		}
	}

	// Destroy tokenizer
	tokenizer_destroy(&tokenizer);

	// Destroy label record list
	bool undefined_label = false;
	for (; label_recs;) {
		// Check for undefined labels.
		// It's not worth printing these errors if another error already occurred, because
		// the other error may have halted assembly before the labels were defined.
		if (error == 0 && label_recs->usages) {
			if (label_recs->string_lit) {
				stasm_msgf(SMT_ERROR, "use of undefined string literal");
			}
			else {
				stasm_msgf(SMT_ERROR, "use of undefined label \"%s\"", label_recs->label);
			}
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
	*/
}
