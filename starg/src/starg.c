// starg.c
// Command line argument parsing

#include "starg.h"

#include <string.h>

// Return a brief description of the given argument error
static const char *desc_for_arg_error(enum arg_error error)
{
	switch (error) {
	case ARG_ERROR_NONE:
		return "no error";
	case ARG_ERROR_MISSING_REQUIRED_ARGUMENT:
		return "missing required argument";
	case ARG_ERROR_MISSING_NAMED_VALUE:
		return "missing named value";
	case ARG_ERROR_UNEXPECTED_ARGUMENT:
		return "unexpected argument";
	case ARG_ERROR_UNRECOGNIZED_FLAG:
		return "unrecognized flag";
	case ARG_ERROR_UNEXPECTED_NAMED_ARGUMENT_FLAG:
		return "unexpected named argument flag";
	}
	return "UNKNOWN";
}

bool print_arg_error(enum arg_error error, const char *arg)
{
	int ret = fprintf(stderr, "error: %s", desc_for_arg_error(error));
	if (ret < 0) {
		return ret;
	}
	if (arg == NULL) {
		ret = fprintf(stderr, "\n");
	} else {
		ret = fprintf(stderr, ": %s\n", arg);
	}
	return ret >= 0; // Continue to parse arguments as long as print was successful
}

// Returns the first non-positional argument description in the given list which matches the given name.
// Returns NULL if there is no such match.
static struct arg_desc *find_name_match(
	struct arg_desc *desc,
	const char *name)
{
	for (; desc->type != ARG_TYPE_NONE; desc++) {
		if (desc->type != ARG_TYPE_POSITIONAL &&
			desc->name != NULL && strcmp(desc->name, name) == 0) {
			return desc;
		}
	}
	return NULL;
}

// Returns the first non-positional argument description in the given list which matches the given flag.
// Returns NULL if there is no such match.
static struct arg_desc *find_flag_match(
	struct arg_desc *desc,
	char flag)
{
	for (; desc->type != ARG_TYPE_NONE; desc++) {
		if (desc->type != ARG_TYPE_POSITIONAL && desc->flag == flag) {
			return desc;
		}
	}
	return NULL;
}

enum arg_error parse_args(
	struct arg_desc *descs,
	arg_handler handler,
	arg_error_handler error_handler,
	int argc,
	const char *argv[]
) {
	// Whether the current context only allows positional arguments
	bool positional_only = false;
	// Pointer to the last positional argument description used
	struct arg_desc *positional_desc = NULL;
	// Pointer to the expected named argument, if any
	struct arg_desc *named_expected = NULL;

	enum arg_error last_error = ARG_ERROR_NONE;

	// Current command-line argument value
	const char *arg = NULL;

	// Iterate through command-line arguments, skipping program name
	for (int i = 1; i < argc; i++) {
		arg = argv[i];

		if (!positional_only) {
			if (named_expected != NULL) {
				// A named argument value is expected in this context
				if (named_expected->value != NULL) {
					// Store argument value
					*named_expected->value = arg;
				}
				if (handler != NULL) {
					// Invoke handler
					handler(named_expected, arg);
				}
				named_expected = NULL;
				continue;
			}

			// Only positional arguments follow "--"
			if (strcmp(arg, "--") == 0) {
				positional_only = true;
				continue;
			}

			// Argument may be a named or unary argument. Check for name match.
			struct arg_desc *desc = find_name_match(descs, arg);
			if (desc != NULL) {
				// Name match found
				if (desc->type == ARG_TYPE_UNARY) {
					// Argument is unary argument
					if (desc->value != NULL) {
						// Store argument which set this unary argument
						*desc->value = arg;
					}
					if (handler != NULL) {
						// Invoke handler
						handler(desc, arg);
					}
				}
				else if (desc->type == ARG_TYPE_NAMED) {
					// Argument is named argument. Expect a value next.
					named_expected = desc;
				}
				continue;
			}

			if (arg[0] == '-' && arg[1] != '\0' && arg[1] != '-') {
				// This argument starts with '-' and will be processed as a sequence
				// of single-character flags
				for (const char *flag = arg + 1; *flag != '\0'; flag++) {
					// Check for flag match
					desc = find_flag_match(descs, *flag);
					if (desc == NULL) {
						// Unrecognized flag
						last_error = ARG_ERROR_UNRECOGNIZED_FLAG;
						if (error_handler == NULL || !error_handler(last_error, arg)) {
							return last_error;
						}
						continue;
					}

					// Flag matches
					if (desc->type == ARG_TYPE_UNARY) {
						if (desc->value != NULL) {
							// Store argument which set this unary argument
							*desc->value = arg;
						}
						if (handler != NULL) {
							// Invoke handler
							handler(desc, arg);
						}
					}
					else if (desc->type == ARG_TYPE_NAMED) {
						// Argument is named argument. Expect a value next.
						if (named_expected != NULL) {
							// More than one named argument flag appears in this list of flags.
							// Only one named argument flag may appear in a list of flags.
							last_error = ARG_ERROR_UNEXPECTED_NAMED_ARGUMENT_FLAG;
							if (error_handler == NULL || !error_handler(last_error, arg)) {
								return last_error;
							}
						}
						named_expected = desc;
					}
				}

				// Argument has been parsed as sequence of flags
				continue;
			}
		}

		// Argument is positional
		if (positional_desc == NULL ||
			(positional_desc->value != NULL && *positional_desc->value != NULL)) {
			// Last-used positional argument description does not exist or cannot be reused.
			// Advance to next positional argument description.
			if (positional_desc == NULL) {
				// Start at the beginning
				positional_desc = descs;
			}
			for (; positional_desc->type != ARG_TYPE_POSITIONAL &&
				positional_desc->type != ARG_TYPE_NONE;
				positional_desc++);
		}
		if (positional_desc->type == ARG_TYPE_NONE) {
			// There are no remaining usable positional argument descriptions
			last_error = ARG_ERROR_UNEXPECTED_ARGUMENT;
			if (error_handler == NULL || !error_handler(last_error, arg)) {
				return last_error;
			}
			continue;
		}

		if (positional_desc->value != NULL) {
			// Store argument value
			*positional_desc->value = arg;
		}
		if (handler != NULL) {
			handler(positional_desc, arg);
		}
	}

	if (named_expected != NULL) {
		// Missing named argument value
		last_error = ARG_ERROR_MISSING_NAMED_VALUE;
		if (error_handler == NULL || !error_handler(last_error, arg)) {
			return last_error;
		}
	}

	// Check for missing required arguments
	for (const struct arg_desc *desc = descs; desc->type != ARG_TYPE_NONE; desc++) {
		if (desc->required && desc->value != NULL && *desc->value == NULL) {
			// A required argument was not specified
			last_error = ARG_ERROR_MISSING_REQUIRED_ARGUMENT;
			if (error_handler == NULL || !error_handler(last_error, desc->value_hint)) {
				return last_error;
			}
		}
	}

	return last_error;
}

int print_usage(const char *pgm, const struct arg_desc *descs)
{
	int ret = printf("usage: %s", pgm);
	if (ret < 0) {
		return ret;
	}

	// Print brief explanation of each argument
	for (const struct arg_desc *desc = descs; desc->type != ARG_TYPE_NONE; desc++) {
		const char *value_hint;
		if (desc->value_hint == NULL) {
			value_hint = "<val>";
		}
		else {
			value_hint = desc->value_hint;
		}

		if (desc->type == ARG_TYPE_UNARY) {
			if (desc->required) {
				if (desc->name == NULL) {
					if (desc->flag != '\0') {
						ret = printf(" -%c", desc->flag);
						if (ret < 0) {
							return ret;
						}
					}
				}
				else {
					if (desc->flag == '\0') {
						ret = printf(" %s", desc->name);
						if (ret < 0) {
							return ret;
						}
					}
					else {
						ret = printf(" <%s | -%c>", desc->name, desc->flag);
						if (ret < 0) {
							return ret;
						}
					}
				}
			}
			else {
				if (desc->name == NULL) {
					if (desc->flag != '\0') {
						ret = printf(" [-%c]", desc->flag);
						if (ret < 0) {
							return ret;
						}
					}
				}
				else {
					if (desc->flag == '\0') {
						ret = printf(" [%s]", desc->name);
						if (ret < 0) {
							return ret;
						}
					}
					else {
						ret = printf(" [%s | -%c]", desc->name, desc->flag);
						if (ret < 0) {
							return ret;
						}
					}
				}
			}
		}
		else if (desc->type == ARG_TYPE_NAMED) {

			if (desc->required) {
				if (desc->name == NULL) {
					if (desc->flag != '\0') {
						ret = printf(" -%c %s", desc->flag, value_hint);
						if (ret < 0) {
							return ret;
						}
					}
				}
				else {
					if (desc->flag == '\0') {
						ret = printf(" %s %s", desc->name, value_hint);
						if (ret < 0) {
							return ret;
						}
					}
					else {
						ret = printf(" <%s %s | -%c %s>", desc->name, value_hint, desc->flag, value_hint);
						if (ret < 0) {
							return ret;
						}
					}
				}
			}
			else {
				if (desc->name == NULL) {
					if (desc->flag != '\0') {
						ret = printf(" [-%c %s]", desc->flag, value_hint);
						if (ret < 0) {
							return ret;
						}
					}
				}
				else {
					if (desc->flag == '\0') {
						ret = printf(" [%s %s]", desc->name, value_hint);
						if (ret < 0) {
							return ret;
						}
					}
					else {
						ret = printf(" [%s %s | -%c %s]", desc->name, value_hint, desc->flag, value_hint);
						if (ret < 0) {
							return ret;
						}
					}
				}
			}
		}
		else if (desc->type == ARG_TYPE_POSITIONAL) {
			if (desc->required) {
				if (desc->value == NULL) {
					ret = printf(" ...");
					if (ret < 0) {
						return ret;
					}
				}
				else {
					ret = printf(" %s", value_hint);
					if (ret < 0) {
						return ret;
					}
				}
			}
			else {
				if (desc->value == NULL) {
					ret = printf(" [...]");
					if (ret < 0) {
						return ret;
					}
				}
				else {
					ret = printf(" [%s]", value_hint);
					if (ret < 0) {
						return ret;
					}
				}
			}
		}
	}
	printf("\n");

	return 0;
}
