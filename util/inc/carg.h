// carg.h
// Command-line argument parsing

#pragma once

#include <stdbool.h>
#include <stdio.h>

// Enumerates the errors which may occur while parsing arguments
enum carg_error
{
	CARG_ERROR_NONE = 0,
	CARG_ERROR_MISSING_REQUIRED_ARGUMENT,
	CARG_ERROR_MISSING_NAMED_VALUE,
	CARG_ERROR_UNEXPECTED_ARGUMENT,
	CARG_ERROR_UNRECOGNIZED_FLAG,
	CARG_ERROR_UNRECOGNIZED_OPTION,
};

// Handler for errors that may occur when parsing command-line arguments.
// Called with the type of error and the argument which caused it.
// Returns whether the application should continue to parse arguments.
typedef bool (*carg_error_handler)(enum carg_error, const char *arg);

// Prints a description of the given argument error to stderr.
// Useful as a typical argument error handler.
bool carg_print_error(enum carg_error, const char *arg);

// Enumerates the types of arguments which the program may accept
enum carg_type
{
	CARG_TYPE_NONE,
	CARG_TYPE_UNARY,
	CARG_TYPE_NAMED,
	CARG_TYPE_POSITIONAL,
};

// Structure which describes an argument the program may accept
struct carg_desc
{
	// The type of this argument
	enum carg_type type;
	// A single character flag for this argument or '\0' if there is none
	char flag;
	// The name of the argument. For instance, "--foo" or "--bar".
	// NULL if the argument does not have a name.
	const char *name;
	// For named and positional arguments, a pointer to the value of the argument.
	// For unary arguments, a pointer to the command-line argument in which it was set,
	// if it was set.
	// NULL if the argument does not store a value.
	const char **value;
	// Whether this argument is required
	bool required;
	// Text describing the argument as would be printed in a usage statement.
	// NULL if the argument does not have help text.
	const char *usage;
	// Brief description of the value. Used in usage text.
	const char *value_hint;
};

// Handler for arguments encountered when parsing command-line arguments.
// Called with the description of the argument and the argument value.
typedef void (*carg_handler)(struct carg_desc *desc, const char *arg);

// Parses command-line arguments as they are received by the program based on the given array
// of argument descriptions.
// Returns an error code.
enum carg_error carg_parse_args(
	// Array of descriptions of arguments accepted by the program terminated by
	// an entry with type ARG_TYPE_NONE
	struct carg_desc *descs,
	// Handler to be invoked for arguments encountered during parsing or NULL
	carg_handler handler,
	// Handler to be invoked for errors that occur during parsing or NULL
	carg_error_handler error_handler,
	// Number of command-line arguments as received by the program
	int argc,
	// Array of command-line argument values as received by the program
	const char *argv[]
);

// Prints a usage statement to stdout based on the given list of possible arguments.
// Returns zero on success, non-zero on error.
int carg_print_usage(const char *pgm, const struct carg_desc *descs);
