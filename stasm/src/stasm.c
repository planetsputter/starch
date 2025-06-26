// stasm.c

#include <errno.h>
#include <stdlib.h>

#include "bstr.h"
#include "carg.h"
#include "parser.h"
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
void detect_non_help_arg(struct carg_desc *desc, const char*)
{
	if (desc->value != &arg_help) {
		non_help_arg = true;
	}
}

// Include file link
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

// Label definition
struct label_def {
	char *label; // B-string
	uint64_t addr;
	struct label_def *prev;
};

// Initializes the given label definition, taking ownership of the given label B-string
static void label_def_init(struct label_def *ld, uint64_t addr, char *label)
{
	ld->label = label;
	ld->addr = addr;
	ld->prev = NULL;
}

// Destroys the given label definition
static void label_def_destroy(struct label_def *ld)
{
	bstr_free(ld->label);
	ld->label = NULL;
}

// Look up the label definition for the given label name
static struct label_def *get_def_for_label(struct label_def *defs, const char *label)
{
	for (; defs; defs = defs->prev) {
		if (strcmp(defs->label, label) == 0) {
			return defs;
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
	FILE *outfile = fopen(arg_output, "wb");
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

	// Initialize label definition list. @todo: Make a binary search tree for efficient lookup.
	struct label_def *label_defs = NULL;

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
				fprintf(stderr, "error: failed to get offset of section %d with error %d\n", sec_count, error);
				error = 1;
			}
			else {
				sec_count++;
			}
			break;

		//
		// Instruction
		//
		case PET_INST:
			if (sec_count == 0) {
				fprintf(stderr, "error: expected section definition before first instruction\n");
				error = 1;
				break;
			}

			// Write instruction to output file
			uint8_t buff[9]; // Maximum instruction length is 9
			if (pe.inst.immlen > sizeof(buff)) {
				fprintf(stderr, "error: immediate length too long at %d bytes\n", pe.inst.immlen);
				error = 1;
			}
			else if (pe.inst.imm_label != NULL) {
				// @todo: handle instructions with label immediates
				fprintf(stderr, "error: instructions with label immediates not yet supported\n");
				error = 1;
			}
			else {
				buff[0] = pe.inst.opcode;
				memcpy(buff + 1, pe.inst.imm, pe.inst.immlen);
				size_t written = fwrite(buff, 1, pe.inst.immlen + 1, outfile);
				if (written != (size_t)pe.inst.immlen + 1) {
					fprintf(stderr, "error: failed to write to output file %s, errno %d\n", arg_output, errno);
					error = 1;
				}
			}
			break;

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
			}
			else if (get_def_for_label(label_defs, pe.label)) {
				// A definition already exists for that label
				fprintf(stderr, "error: definition already exists for label %s\n", pe.label);
				error = 1;
			}
			else {
				// Add new label definition to list
				uint64_t addr = fpos - curr_sec_fo + curr_sec.addr;
				struct label_def *next = (struct label_def*)malloc(sizeof(struct label_def));
				label_def_init(next, addr, pe.label);
				pe.label = NULL;
				next->prev = label_defs;
				label_defs = next;
			}
		}	break;

		default: // Invalid event type
			fprintf(stderr, "error: invalid parser event type %d\n", pe.type);
			error = 1;
			break;
		}

		parser_event_destroy(&pe); // Destroy the parser event
	}

	if (!error && sec_count) {
		error = stub_save_section(outfile, sec_count - 1, &curr_sec);
	}

	// Destroy label definition list
	for (; label_defs;) {
		struct label_def *prev = label_defs->prev;
		label_def_destroy(label_defs);
		label_defs = prev;
	}

	// Destroy include chain
	for (; inc_chain; ) {
		struct inc_link *prev = inc_chain->prev;
		inc_link_destroy(inc_chain);
		inc_chain = prev;
	}

	// Close output file
	fclose(outfile);

	return error;
}
