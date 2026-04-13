// stasm.c

#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "assembler.h"
#include "bstr.h"
#include "carg.h"
#include "lits.h"
#include "starch.h"
#include "stmsg.h"
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
		'h',             // Flag
		"--help",        // Name
		&arg_help,       // Value
		false,           // Required
		"show usage",    // Usage text
		NULL             // Value hint
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
		'\0',            // Flag
		"--maxnsec",     // Name
		&arg_maxnsec,    // Value
		false,           // Required
		"maximum number of sections", // Usage text
		"maxnsec"        // Value hint
	},
	{
		CARG_TYPE_NAMED, // Type
		'I',             // Flag
		"--include",     // Name
		NULL,            // Value
		false,           // Required
		"directory", // Usage text
		"directory" // Value hint
	},
	{
		CARG_TYPE_POSITIONAL, // Type
		'\0',                 // Flag
		NULL,                 // Name
		&arg_src,             // Value
		false,                // Required
		"starch source",      // Usage text
		"source"              // Value hint
	},
	{ CARG_TYPE_NONE }
};

//
// Included directory list
//
struct inc_dir {
	bchar *dir;
	struct inc_dir *next;
};
static void inc_dir_destroy(struct inc_dir *inc_dir)
{
	if (inc_dir->dir) {
		bfree(inc_dir->dir);
		inc_dir->dir = NULL;
	}
}
static struct inc_dir *inc_dirs;

static bool non_help_arg = false;
static void handle_arg(struct carg_desc *desc, const char *arg)
{
	if (desc->value == &arg_help) {
		return;
	}
	non_help_arg = true;

	if (desc->value == &arg_maxnsec) {
		// If a maxnsec argument is given, attempt to parse as an integer
		char *end = NULL;
		maxnsec = strtol(arg, &end, 0);
		if (!end || *end != '\0' || *arg == '\0') {
			stmsgf(SMT_ERROR, "failed to parse --maxnsec argument \"%s\"", arg_maxnsec);
		}
		else if (maxnsec <= 0) {
			stmsgf(SMT_ERROR, "invalid --maxnsec value %d", maxnsec);
		}
	}
	else if (desc->value == &arg_output) {
		// Output file name must not be empty or end in a slash
		if (arg[0] == '\0' || arg[strlen(arg) - 1] == '/') {
			stmsgf(SMT_ERROR, "invalid output file name \"%s\"", arg);
		}
	}
	else if (desc->name && strcmp(desc->name, "--include") == 0) {
		if (arg[0] == '\0') {
			stmsgf(SMT_ERROR, "invalid include directory name \"\"");
		}
		else {
			// Add include directory to front of list
			struct inc_dir *inc_dir = (struct inc_dir*)malloc(sizeof(struct inc_dir));
			inc_dir->dir = bstrdupc(arg);
			if (inc_dir->dir[bstrlen(inc_dir->dir) - 1] != '/') {
				inc_dir->dir = bstrcatc(inc_dir->dir, "/");
			}
			inc_dir->next = inc_dirs;
			inc_dirs = inc_dir;
		}
	}
}

int main(int argc, const char *argv[])
{
	int ret = 0;

	// Parse command-line arguments
	enum carg_error parse_error = carg_parse_args(
		arg_descs,
		handle_arg,
		carg_print_error,
		argc,
		argv
	);
	if (arg_help) {
		// Usage requested
		if (non_help_arg) {// Other arguments present
			stmsgf(SMT_ERROR, "Only printing usage. Other arguments present.");
			ret = 1;
		}
		carg_print_usage(argv[0], arg_descs);
		return ret;
	}
	if (parse_error != CARG_ERROR_NONE || stmsg_counts[SMT_ERROR]) {
		// Error message has already been printed
		return 1;
	}
	// Arguments parsed, no errors

	// Open input file
	FILE *infile;
	if (arg_src) {
		infile = fopen(arg_src, "r");
		if (!infile) {
			stmsgf(SMT_ERROR, "failed to open \"%s\", errno %d", arg_src, errno);
			return 1;
		}
	}
	else {
		infile = stdin;
	}

	// Attempt to remove output file, ignoring failure
	remove(arg_output);

	// Open output file with ".tmp" extension
	bchar *outfilename = bstrcatc(bstrdupc(arg_output), ".tmp");
	FILE *outfile = fopen(outfilename, "w+b");
	if (!outfile) {
		if (infile != stdin) {
			fclose(infile);
		}
		stmsgf(SMT_ERROR, "failed to open \"%s\" for writing, errno %d", outfilename, errno);
		bfree(outfilename);
		return 1;
	}

	// Initialize output file
	ret = stub_init(outfile, maxnsec);
	if (ret) {
		fclose(outfile);
		if (infile != stdin) {
			fclose(infile);
		}
		stmsgf(SMT_ERROR, "failed to initialize output stub file \"%s\"", outfilename);
		bfree(outfilename);
		return 1;
	}

	// Initialize include chain
	inc_chain = (struct inc_link*)malloc(sizeof(struct inc_link));
	inc_link_init(inc_chain, strdup(arg_src ? arg_src : "(stdin)"), infile);

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

	// Token line and character number
	int tlineno = 0, tcharno = 0;
	// Current line and character number
	int lineno = 1, charno = 0;

	// Parse each byte of the source file and each included file
	while (inc_chain) {
		// Check whether a token is in progress
		if (!tip && tokenizer_in_progress(&tokenizer)) {
			tip = true;
			tlineno = lineno;
			tcharno = charno;
		}

		// Assemble any emitted tokens
		bchar *token = NULL;
		tokenizer_emit(&tokenizer, &token);
		if (token) {
			tip = false;
			// Assemble the token
			ret = assembler_handle_token(&as, token, tlineno, tcharno);
			if (ret) break;

			// Check for included file
			bchar *filename = NULL;
			assembler_get_include(&as, &filename);
			if (filename) {
				// Check that filename does not include an embedded null byte
				size_t ni = bstrfindbyte(filename, '\0');
				if (ni == bstrlen(filename)) {
					stmsgtf(SMT_ERROR, tlineno, tcharno, "embedded null in include file name");
					ret = 1;
					break;
				}

				// Determine path to included file
				struct stat inc_stat;
				int statret;
				struct inc_dir *inc_dir = NULL;
				bchar *path = NULL;
				do {
					if (path) {
						bfree(path);
					}
					if (inc_dir) {
						path = bstrcatb(bstrdupb(inc_dir->dir), filename);
						inc_dir = inc_dir->next;
					}
					else {
						path = bstrdupb(filename);
						inc_dir = inc_dirs;
					}
					statret = stat(path, &inc_stat);
					if (statret != 0) {
						statret = errno;
					}
				} while (statret == ENOENT && inc_dir);

				if (statret == ENOENT) {
					// Failed to find file
					stmsgtf(SMT_ERROR, tlineno, tcharno, "failed to find included file \"%s\"", filename);
					ret = 1;
					bfree(path);
					bfree(filename);
					break;
				}
				bfree(filename);

				if (statret != 0) {
					// The file exists, but we failed to stat it
					stmsgtf(SMT_ERROR, tlineno, tcharno, "failed to stat file \"%s\", errno %d", path, statret);
					ret = 1;
					bfree(path);
					break;
				}

				// Open included file
				FILE *incfile = fopen(path, "r");
				if (!incfile) {
					stmsgtf(SMT_ERROR, tlineno, tcharno, "failed to open \"%s\", errno %d", path, errno);
					ret = 1;
					bfree(path);
					break;
				}

				// Add link to chain
				inc_chain->lineno = lineno;
				inc_chain->charno = charno;
				struct inc_link *link = (struct inc_link*)malloc(sizeof(struct inc_link));
				inc_link_init(link, strdup(path), incfile);
				bfree(path);
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
			lineno = inc_chain->lineno;
			charno = inc_chain->charno;
		}
		// Keep track of line and character numbers
		else if (c == '\n') {
			lineno++;
			charno = 1;
		}
		else {
			charno++;
		}

		// Get a character from the current input file
		for (;;) {
			int b = fgetc(inc_chain->file);
			if (b == EOF) { // Finish file
				if (ferror(inc_chain->file)) {// Check for file read error
					stmsgf(SMT_ERROR, "read failed, errno %d", errno);
					ret = 1;
					break;
				}
				// Ensure the decoder and tokenizer can terminate now
				if (!utf8_decoder_can_terminate(&decoder) || !tokenizer_finish(&tokenizer)) {
					stmsgtf(SMT_ERROR, lineno, charno, "unexpected EOF");
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
				stmsgf(SMT_ERROR, "encoding error");
				break;
			}
			if (cp != &c) break; // Character generated
		}
		if (ret) break;

		// Feed the generated character to the tokenizer
		ret = tokenizer_parse(&tokenizer, c);
		if (ret) break;
	}

	int finret = assembler_finish(&as, lineno, charno);
	if (ret == 0) {
		ret = finret;
	}

	if (as.sec_count) {
		// Save the last section of the stub file
		int save_error = stub_save_section(outfile, as.sec_count - 1, &as.curr_sec);
		if (save_error) {
			stmsgf(SMT_ERROR, "failed to save section %d to \"%s\"", as.sec_count - 1, outfilename);
			if (ret == 0) {
				ret = save_error;
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

	// Destroy included directory list
	while (inc_dirs) {
		struct inc_dir *next = inc_dirs->next;
		inc_dir_destroy(inc_dirs);
		free(inc_dirs);
		inc_dirs = next;
	}

	// Close output file
	fclose(outfile);

	if (ret == 0) {
		// Fail if any error messages were emitted
		if (stmsg_counts[SMT_ERROR] != 0) {
			ret = 1;
		}
		// On success, copy temporary output to named output
		else {
			ret = rename(outfilename, arg_output);
			if (ret) {
				stmsgf(SMT_ERROR, "failed to move \"%s\" to \"%s\"", outfilename, arg_output);
			}
		}
	}
	bfree(outfilename);

	return ret;
}
