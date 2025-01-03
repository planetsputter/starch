// starg.h
// Command-line argument parsing

#pragma once

#include <stdbool.h>
#include <stdio.h>

// Enumerates the errors which may occur while parsing arguments
enum arg_error
{
	ARG_ERROR_NONE = 0,
	ARG_ERROR_MISSING_REQUIRED_ARGUMENT,
	ARG_ERROR_MISSING_NAMED_VALUE,
	ARG_ERROR_UNEXPECTED_ARGUMENT,
	ARG_ERROR_UNRECOGNIZED_FLAG,
	ARG_ERROR_UNEXPECTED_NAMED_ARGUMENT_FLAG,
};

// Handler for errors that may occur when parsing command-line arguments.
// Called with the type of error and the argument which caused it.
// Returns whether the application should continue to parse arguments.
typedef bool (*arg_error_handler)(enum arg_error, const char *arg);

// Prints a description of the given argument error to stderr.
// Useful as a typical argument error handler.
bool print_arg_error(enum arg_error, const char *arg);

// Enumerates the types of arguments which the program may accept
enum arg_type
{
	ARG_TYPE_NONE,
	ARG_TYPE_UNARY,
	ARG_TYPE_NAMED,
	ARG_TYPE_POSITIONAL,
};

// Structure which describes an argument the program may accept
struct arg_desc
{
	// The type of this argument
	enum arg_type type;
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
typedef void (*arg_handler)(struct arg_desc *desc, const char *arg);

// Parses command-line arguments as they are received by the program based on the given array
// of argument descriptions.
// Returns an error code.
enum arg_error parse_args(
	// Array of descriptions of arguments accepted by the program terminated by
	// an entry with type ARG_TYPE_NONE
	struct arg_desc *descs,
	// Handler to be invoked for arguments encountered during parsing or NULL
	arg_handler handler,
	// Handler to be invoked for errors that occur during parsing or NULL
	arg_error_handler error_handler,
	// Number of command-line arguments as received by the program
	int argc,
	// Array of command-line argument values as received by the program
	const char *argv[]
);

// Prints a usage statement to stdout based on the given list of possible arguments.
// Returns zero on success, non-zero on error.
int print_usage(const char *pgm, const struct arg_desc *descs);
