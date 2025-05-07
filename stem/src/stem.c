// stem.c

#include <errno.h>
#include <stdlib.h>

#include "core.h"
#include "mem.h"
#include "starch.h"
#include "carg.h"
#include "stub.h"

// Variables set by command-line arguments
const char *arg_cycles = NULL;
const char *arg_dump = NULL;
const char *arg_help = NULL;
const char *arg_image = NULL;

struct carg_desc arg_descs[] = {
	{
		CARG_TYPE_UNARY,
		'h',
		"--help",
		&arg_help,
		false,
		"show usage",
		NULL
	},
	{
		CARG_TYPE_NAMED,
		'c',
		"--cycles",
		&arg_cycles,
		false,
		"cycles",
		"cycles"
	},
	{
		CARG_TYPE_NAMED,
		'd',
		"--dump",
		&arg_dump,
		false,
		"hex dump",
		"dump"
	},
	{
		CARG_TYPE_POSITIONAL,
		'\0',
		NULL,
		&arg_image,
		true,
		"starch image",
		"image"
	},
	{ CARG_TYPE_NONE }
};

bool non_help_arg = false;
void detect_non_help_arg(struct carg_desc *desc, const char*)
{
	if (desc->value != &arg_help) {
		non_help_arg = true;
	}
}

int main(int argc, const char *argv[])
{
	// Parse command-line arguments
	enum carg_error parse_error = carg_parse_args(
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
		carg_print_usage(argv[0], arg_descs);
		return parse_error != CARG_ERROR_NONE;
	}
	if (parse_error != CARG_ERROR_NONE) {
		// Parse arguments again, printing errors
		parse_error = carg_parse_args(
			arg_descs,
			NULL,
			carg_print_error,
			argc,
			argv
		);
		return 1;
	}
	// Arguments parsed, no errors

	long int max_cycles = -1;
	if (arg_cycles) {
		char *endptr = NULL;
		max_cycles = strtol(arg_cycles, &endptr, 0);
		if (*arg_cycles == '\0' || *endptr != '\0') {
			fprintf(stderr, "error: invalid cycle count \"%s\"\n", arg_cycles);
			return 1;
		}
	}

	// Open the input image file
	FILE *infile = fopen(arg_image, "rb");
	if (infile == NULL) {
		fprintf(stderr, "error: failed to open image file \"%s\"\n", arg_image);
		return errno;
	}

	// Verify the image file is a stub file
	int ret = stub_verify(infile);
	if (ret) {
		fclose(infile);
		fprintf(stderr, "error: \"%s\" is not a valid stub file\n", arg_image);
		return ret;
	}

	// Initialize emulated memory. Give ourselves 1 GiB.
	struct mem memory;
	mem_init(&memory, 0x40000000);

	// Initialize a core
	struct core core;
	core_init(&core);

	// Load first stub section information
	struct stub_sec sec;
	ret = stub_load_section(infile, 0, &sec);
	if (ret) {
		core_destroy(&core);
		fclose(infile);
		fprintf(stderr, "error: failed to load section 0 from \"%s\"\n", arg_image);
		return ret;
	}

	// Load input file into memory
	ret = mem_load_image(&memory, sec.addr, sec.size, infile);
	if (ret) {
		fprintf(stderr, "error: failed to load memory from \"%s\" to address 0\n", arg_image);
	}
	else {
		for (int cycles = 0; (max_cycles < 0 || cycles < max_cycles) && ret == 0; cycles++) {
			ret = core_step(&core, &memory);
		}
		// Print an error message for non-halt error codes
		if (ret != STERR_HALT && ret != STERR_NONE) {
			fprintf(stderr, "error: an error occurred during emulation: %s (%d)\n", name_for_sterr(ret), ret);
		}
	}

	// Create a hex dump if requested
	if (arg_dump) {
		FILE *dumpfile = fopen(arg_dump, "wb");
		if (!dumpfile) {
			fprintf(stderr, "error: unable to open hex dump file \"%s\"\n", arg_dump);
			ret = 1;
		}
		else {
			ret = mem_dump_hex(&memory, 0, 0, dumpfile);
			fclose(dumpfile);
		}
	}

	// Clean up
	mem_destroy(&memory);
	core_destroy(&core);
	fclose(infile);
	return ret == STERR_HALT || ret == STERR_NONE ? 0 : ret;
}
