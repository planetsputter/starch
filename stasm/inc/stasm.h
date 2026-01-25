// stasm.h

#pragma once

#include <stdbool.h>

enum {
	// Stasm message types
	SMT_INFO,
	SMT_WARN,
	SMT_ERROR,
	SMT_NUM, // Number of message types

	// Stasm message flags
	SMF_USETOK = 0x80,
};

// Prints the given formatted message to stdout for info messages or stderr
// for warnings and errors. Returns the result of the printf-style call.
// Prepends the message type, current file name, line number, and character number if known.
// If msg_type & SMF_USETOK, uses current token line and character numbers instead.
// After the message prints the include chain if known.
int stasm_msgf(int msg_type, const char *format, ...);

// Returns the number of messages of the given type
size_t stasm_count_msg(int msg_type);
