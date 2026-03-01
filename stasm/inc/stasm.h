// stasm.h

#pragma once

#include <stdbool.h>

enum {
	// Stasm message types
	SMT_INFO,
	SMT_WARN,
	SMT_ERROR,
	SMT_NUM, // Number of message types
};

// Prints the given formatted message to stdout for info messages or stderr
// for warnings and errors. Returns the result of the printf-style call.
// Prepends the message type and current file name.
// After the message prints the include chain if known.
int stasm_msgf(int msg_type, const char *format, ...);

// Same as above, but includes token line and character information in the message prefix
int stasm_msgft(int msg_type, int lineno, int charno, const char *format, ...);

// Returns the number of messages of the given type
size_t stasm_count_msg(int msg_type);
