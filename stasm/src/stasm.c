// stasm.c

#include "parser.h"
#include "starg.h"
#include "stub.h"

// Variables set by command-line arguments
const char *arg_help = NULL;
const char *arg_src = NULL;
const char *arg_output = "a.stb";

struct arg_desc arg_descs[] = {
	{
		ARG_TYPE_UNARY, // Type
		'h',            // Flag
		"--help",       // Name
		&arg_help,      // Value
		false,          // Required
		"show usage",   // Usage text
		NULL            // Value hint
	},
	{
		ARG_TYPE_NAMED,  // Type
		'o',             // Flag
		"--output",      // Name
		&arg_output,     // Value
		false,           // Required
		"binary output", // Usage text
		"output"         // Value hint
	},
	{
		ARG_TYPE_POSITIONAL, // Type
		'\0',                // Flag
		NULL,                // Name
		&arg_src,            // Value
		false,               // Required
		"starch source",     // Usage text
		"source"             // Value hint
	},
	{ ARG_TYPE_NONE }
};

bool non_help_arg = false;
void detect_non_help_arg(struct arg_desc *desc, const char*)
{
	if (desc->value != &arg_help) {
		non_help_arg = true;
	}
}

int main(int argc, const char *argv[])
{
	// Parse command-line arguments
	enum arg_error parse_error = parse_args(
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
		print_usage(argv[0], arg_descs);
		return parse_error != ARG_ERROR_NONE;
	}
	if (parse_error != ARG_ERROR_NONE) {
		// Parse arguments again, printing errors
		parse_error = parse_args(
			arg_descs,
			NULL,
			print_arg_error,
			argc,
			argv
		);
		return 1;
	}
	// Arguments parsed, no errors

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

	struct Parser parser;
	Parser_Init(&parser);
	int error = 0, c;
	while ((c = fgetc(infile)) != EOF) {
		Parser_ParseByte(&parser, c, &error);
		if (error) break;
	}
	if (!error && ferror(infile)) {
		fprintf(stderr, "error: failed to read from %s\n", arg_src ? arg_src : "stdin");
		error = 1;
	}
	if (infile != stdin) {
		fclose(infile);
	}
	if (!error) {
		if (!Parser_CanTerminate(&parser)) {
			fprintf(stderr, "error: unexpected end of file in %s\n", arg_src ? arg_src : "stdin");
			error = 1;
		}
		else {
			error = Parser_Terminate(&parser);
			if (!error) {
				FILE *outfile = fopen(arg_output, "wb");
				if (!outfile) {
					fprintf(stderr, "error: failed to open output file for writing: %s\n", arg_output);
					error = 1;
				}
				else {
					// Initialize output stub file with one section
					error = stub_init(outfile, 1);
					if (error == 0) {
						error = Parser_WriteBytecode(&parser, outfile);
						if (error == 0) {
							// Save the first section
							struct stub_sec sec;
							// @todo: make configurable
							sec.addr = 0x1000;
							sec.flags = STUB_FLAG_TEXT;
							error = stub_save_section(outfile, 0, &sec);
						}
					}
					fclose(outfile);
				}
			}
		}
	}

	Parser_Destroy(&parser);

	if (error) {
		fprintf(stderr, "program exiting with error code %d (%s)\n", error, name_for_sterr(error));
	}
	return error;
}
