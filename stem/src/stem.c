// stem.c

#include "starg.h"
#include "mem.h"

// Variables set by command-line arguments
const char *arg_help = NULL;
const char *arg_image = NULL;

struct arg_desc arg_descs[] = {
	{
		ARG_TYPE_UNARY,
		'h',
		"--help",
		&arg_help,
		false,
		"show usage",
		NULL
	},
	{
		ARG_TYPE_POSITIONAL,
		'\0',
		NULL,
		&arg_image,
		true,
		"starch image",
		"image"
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
			fprintf(stderr, "warning: Only printing usage. Other arguments present\n");
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

	struct mem memory;
	mem_init(&memory);

	mem_destroy(&memory);
	return 0;
}
