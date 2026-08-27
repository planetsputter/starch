// stmsg.c

#include <assert.h>
#include <unistd.h>

#include "stmsg.h"

// The include chain
struct inc_link *inc_chain = NULL;

// The message counts array
size_t stmsg_counts[SMT_NUM];

// Message type names
static const char *stmsg_names[] = {
	"info",
	"warning",
	"error",
};
// Message color codes
#define ANSI_YELLOW "\033[33m"
#define ANSI_RED "\033[31m"
#define ANSI_RESET "\033[0m"
static const char *stmsg_colors[] = {
	"", // Normal
	ANSI_YELLOW,
	ANSI_RED,
};

int stmsgf(int msg_type, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	int ret = vstmsgtf(msg_type, 0, 0, format, args);
	va_end(args);
	return ret;
}

int stmsgtf(int msg_type, int lineno, int charno, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	int ret = vstmsgtf(msg_type, lineno, charno, format, args);
	va_end(args);
	return ret;
}

int vstmsgtf(int msg_type, int lineno, int charno, const char *format, va_list args)
{
	// Determine output file and whether it is a terminal
	assert((size_t)msg_type < sizeof(stmsg_names) / sizeof(*stmsg_names));
	FILE *out_file = msg_type <= SMT_INFO ? stdout : stderr;
	int isterm = isatty(fileno(out_file));

	// Prepend message type name
	fprintf(out_file, "%s%s%s: ", isterm ? stmsg_colors[msg_type] : "",
		stmsg_names[msg_type], isterm ? ANSI_RESET : "");

	// Prepend the current file and line if it is known
	struct inc_link *link = inc_chain;
	if (link && lineno) {
		if (charno) {
			fprintf(out_file, "%s:%d:%d: ", link->filename, lineno, charno);
		}
		else {
			fprintf(out_file, "%s:%d: ", link->filename, lineno);
		}
	}

	// Format and print the given message
	int ret = vfprintf(out_file, format, args);
	fprintf(out_file, "\n"); // Append newline

	// Print the include chain
	if (link && lineno) {
		link = link->prev;
		for (; link; link = link->prev) {
			fprintf(out_file, "  in file included from %s:%d\n", link->filename, link->lineno - 1);
		}
	}

	stmsg_counts[msg_type]++;
	return ret;
}

void inc_link_init(struct inc_link *link, char *filename, FILE *file)
{
	link->filename = filename;
	link->file = file;
	link->lineno = 1;
	link->charno = 0; // Will be incremented before processing first character
	link->prev = NULL;
}

// Destroys the given include link
void inc_link_destroy(struct inc_link *link)
{
	if (link->filename) {
		free(link->filename);
		link->filename = NULL;
	}
	if (link->file) {
		if (link->file != stdin) {
			fclose(link->file);
		}
		link->file = NULL;
	}
}

size_t stmsg_count(int msg_type)
{
	return stmsg_counts[msg_type];
}
