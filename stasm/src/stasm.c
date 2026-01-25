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
			stasm_msgf(SMT_ERROR, "failed to open %s, errno %d", arg_src, errno);
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
		stasm_msgf(SMT_ERROR, "failed to open \"%s\" for writing, errno %d", arg_output, errno);
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
	inc_link_init(inc_chain, arg_src ? bstrdupc(arg_src) : bstrdupc("(stdin)"), infile);

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
					stasm_msgf(SMT_ERROR, "failed to open %s, errno %d", filename, errno);
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

	if (as.sec_count) {
		// Save the last section of the stub file
		int save_error = stub_save_section(outfile, as.sec_count - 1, &as.curr_sec);
		if (save_error) {
			stasm_msgf(SMT_ERROR, "failed to save section %d to \"%s\"", as.sec_count - 1, arg_output);
			if (ret == 0) {
				ret = save_error;
			}
		}
	}

	// Check for undefined labels
	if (ret == 0) {
		for (struct label_rec *rec = as.label_recs; rec; rec = rec->prev) {
			if (!rec->defined) {
				stasm_msgf(SMT_ERROR, "undefined label \"%s\"", rec->label);
				ret = 1;
				break;
			}
		}
	}

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
}
