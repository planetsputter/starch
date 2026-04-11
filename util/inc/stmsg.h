// stmsg.h
//
// Common starch toolchain output message functions.

#pragma once

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

enum { // Starch message types
	SMT_INFO,
	SMT_WARN,
	SMT_ERROR,
	SMT_NUM, // Number of message types
};

// Stasm message counts by type
extern size_t stmsg_counts[SMT_NUM];

// Prints the given formatted message to stdout for info messages or stderr
// for warnings and errors. Returns the result of the printf-style call.
// Prepends the message type.
int stmsgf(int msg_type, const char *format, ...);

// Same as above, but if lineno is non-zero includes token line, character, and filename
// in the message prefix and prints the include chain after the message.
// If charno is zero, only lineno is included.
int stmsgtf(int msg_type, int lineno, int charno, const char *format, ...);

// Same as above, but accepts a variadic argument list
int vstmsgtf(int msg_type, int lineno, int charno, const char *format, va_list args);

// Returns the number of messages of the given type
size_t stmsg_count(int msg_type);

//
// Include file link
//
struct inc_link {
	char *filename;
	FILE *file;
	int lineno, charno;
	struct inc_link *prev;
};

// Initializes the given link, taking ownership of the given filename C-string.
// Also takes ownership of the given file, closing it when the object is destroyed.
void inc_link_init(struct inc_link*, char *filename, FILE *file);

// Destroys the given include link
void inc_link_destroy(struct inc_link*);

// The include chain used for printing messages.
// The caller is responsible for allocating and deallocating the include chain.
extern struct inc_link *inc_chain;
