// stem.c

#include <errno.h>

#include "core.h"
#include "mem.h"
#include "starch.h"
#include "starg.h"

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

	// Open the input image file
	FILE *infile = fopen(arg_image, "rb");
	if (infile == NULL) {
		fprintf(stderr, "error: failed to open image file \"%s\"\n", arg_image);
		return errno;
	}

	// Initialize memory
	struct mem memory;
	mem_init(&memory);

	// Initialize a core
	struct core core;
	core_init(&core);

	// @todo: load input file into memory
	int ret = mem_load_image(&memory, 0, infile);
	if (ret) {
		fprintf(stderr, "error: failed to load memory from \"%s\" to address 0\n", arg_image);
	}
	else {
		for (; (ret = core_step(&core, &memory)) == 0; );
		// @todo: print an error message for non-halt opcodes
		if (ret != STERR_HALT && ret != STERR_NONE) {
			fprintf(stderr, "error: an error occurred during emulation: %s (0x%08x)\n", name_for_sterr(ret), ret);
		}
	}

	// Clean up
	mem_destroy(&memory);
	core_destroy(&core);
	fclose(infile);
	return ret == STERR_HALT || ret == STERR_NONE ? 0 : ret;
}
