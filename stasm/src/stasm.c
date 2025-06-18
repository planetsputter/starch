// stasm.c

#include <errno.h>

#include "parser.h"
#include "carg.h"
#include "stub.h"

// Variables set by command-line arguments
const char *arg_help = NULL;
const char *arg_src = NULL;
const char *arg_output = "a.stb";

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
		CARG_TYPE_NAMED,  // Type
		'o',             // Flag
		"--output",      // Name
		&arg_output,     // Value
		false,           // Required
		"binary output", // Usage text
		"output"         // Value hint
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
	// @todo: Allow to override default maximum number of sections with command-line argument.
	int error = stub_init(outfile, 4);
	if (error) {
		fclose(outfile);
		if (infile != stdin) {
			fclose(infile);
		}
		fprintf(stderr, "error: failed to initialize output stub file \"%s\"\n", arg_output);
		return 1;
	}

	// Set up assembler state
	int sec_count = 0; // Count of sections encountered
	struct stub_sec curr_sec; // The current section
	stub_sec_init(&curr_sec, 0, 0, 0);

	// Parse all bytes of input file until end of file reached or an error occurs
	struct parser parser;
	parser_init(&parser);
	struct parser_event pe;
	parser_event_init(&pe);
	for (; error == 0; ) {
		int c = fgetc(infile);
		if (c == EOF) { // Finish file
			if (ferror(infile)) {
				fprintf(stderr, "error: failed to read from %s, errno %d\n", arg_src ? arg_src : "stdin", errno);
				error = 1;
			}
			else if (!parser_can_terminate(&parser)) {
				fprintf(stderr, "error: unexpected end of file in %s\n", arg_src ? arg_src : "stdin");
				error = 1;
			}
			else {
				error = parser_terminate(&parser, &pe);
			}
		}
		else { // Parse byte
			error = parser_parse_byte(&parser, c, &pe);
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
			}
			else {
				error = stub_load_section(outfile, sec_count, &curr_sec);
				if (error) {
					fprintf(stderr, "error: failed to load stub section %d with error %d\n", sec_count, error);
				}
				else {
					sec_count++;
				}
			}
			break;

		//
		// Instruction
		//
		case PET_INST:
			if (sec_count) {
				// Write instruction to output file
				uint8_t buff[9]; // Maximum instruction length is 9
				if (pe.inst.immlen > sizeof(buff)) {
					fprintf(stderr, "error: immediate length too long at %d bytes\n", pe.inst.immlen);
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
			}
			else {
				fprintf(stderr, "error: expected section definition before first instruction\n");
				error = 1;
			}
			break;

		default: // Invalid event type
			fprintf(stderr, "error: invalid parser event type %d\n", pe.type);
			error = 1;
			break;
		}
		parser_event_init(&pe);

		if (c == EOF) break;
	}

	// Destroy parser
	parser_destroy(&parser);

	// Close output file
	fclose(outfile);

	// Close input file
	if (infile != stdin) {
		fclose(infile);
	}

	return error;
}
