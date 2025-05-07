// stasm.c

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

	// Initialize parser
	struct parser parser;
	int error = parser_init(&parser, outfile);
	if (error) {
		fclose(outfile);
		if (infile != stdin) {
			fclose(infile);
		}
		fprintf(stderr, "error: failed to initialize output stub file \"%s\"\n", arg_output);
		return 1;
	}

	// Parse all bytes of input file
	for (int c; (c = fgetc(infile)) != EOF;) {
		error = parser_parse_byte(&parser, c);
		if (error) break;
	}

	// Check for errors
	if (!error && ferror(infile)) {
		fprintf(stderr, "error: failed to read from %s\n", arg_src ? arg_src : "stdin");
		error = 1;
	}
	if (infile != stdin) {
		fclose(infile);
	}
	if (!error) {
		if (!parser_can_terminate(&parser)) {
			fprintf(stderr, "error: unexpected end of file in %s\n", arg_src ? arg_src : "stdin");
			error = 1;
		}
		else {
			error = parser_terminate(&parser);
			if (error) {
				fprintf(stderr, "error: failed to terminate parser\n");
			}
		}
	}

	parser_destroy(&parser);
	fclose(outfile);

	return error;
}
