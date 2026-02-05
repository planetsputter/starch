// stem.c

#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>

#include "bpmap.h"
#include "core.h"
#include "mem.h"
#include "menu.h"
#include "starch.h"
#include "carg.h"
#include "stem.h"
#include "stub.h"

// Variables set by command-line arguments
const char *arg_cycles = NULL;
const char *arg_dump = NULL;
const char *arg_help = NULL;
const char *arg_image = NULL;
const char *arg_mem_size = NULL;
const char *arg_bp = NULL;

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
		'\0',
		"--mem-size",
		&arg_mem_size,
		false,
		"memory size",
		NULL
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
		CARG_TYPE_NAMED,
		'b',
		"--break",
		&arg_bp,
		false,
		"breakpoint",
		"addr"
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

// The cores of the Starch virtual machine
struct core cores[STEM_NUM_CORES];

// The main memory of the Starch virtual machine (shared among cores)
struct mem main_mem;

// Breakpoint map
struct bpmap *bpmap = NULL;

bool non_help_arg = false, arg_error = false;
void handle_arg(struct carg_desc *desc, const char *arg)
{
	if (desc->value != &arg_help) {
		non_help_arg = true;
	}

	if (!arg_help) {
		if (desc->value == &arg_bp) {
			char *end = NULL;
			long addr = strtol(arg, &end, 0);
			if (end == NULL || *end != '\0' || end == arg) {
				fprintf(stderr, "error: failed to parse BP address: %s\n", arg);
				arg_error = true;
			}
			else {
				if (bpmap == NULL) bpmap = bpmap_create();
				// Note: For now all counts are 1
				bpmap = bpmap_insert(bpmap, addr, 1);
			}
		}
	}
}

int main(int argc, const char *argv[])
{
	// Parse command-line arguments
	enum carg_error parse_error = carg_parse_args(
		arg_descs,
		handle_arg,
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
	if (arg_error) {
		// Error message has already been printed
		return 1;
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

	// Parse number of cycles
	long int max_cycles = -1;
	if (arg_cycles) {
		char *endptr = NULL;
		max_cycles = strtol(arg_cycles, &endptr, 0);
		if (*arg_cycles == '\0' || *endptr != '\0') {
			fprintf(stderr, "error: invalid cycle count \"%s\"\n", arg_cycles);
			return 1;
		}
	}

	// Parse max memory size. Default is 1 GiB.
	long int mem_size = 0x40000000;
	if (arg_mem_size) {
		char *endptr = NULL;
		mem_size = strtol(arg_mem_size, &endptr, 0);
		if (*arg_mem_size == '\0' || *endptr != '\0' || mem_size < 0x4000) {
			fprintf(stderr, "error: invalid memory size \"%s\"\n", arg_mem_size);
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

	// Get number of sections
	int maxnsec = 0, nsec = 0;
	ret = stub_get_section_counts(infile, &maxnsec, &nsec);
	if (ret) {
		fclose(infile);
		fprintf(stderr, "error: failed to get section counts from \"%s\"\n", arg_image);
		return ret;
	}

	// Initialize emulated memory
	mem_init(&main_mem, mem_size);

	// Prepare hard-coded interrupt handlers, which just halt with the interrupt number
	for (int i = 1; i < 256; i++) {
		mem_write8(&main_mem, BEGIN_INT_ADDR + i * 16, op_halt);
		mem_write8(&main_mem, BEGIN_INT_ADDR + i * 16 + 1, i);
	}

	// Initialize the emulated cores
	for (int i = 0; i < STEM_NUM_CORES; i++) {
		core_init(cores + i);
	}

	// Load all sections in input file into memory
	struct stub_sec sec;
	for (int si = 0; si < nsec; si++) {
		// Load section information
		ret = stub_load_section(infile, si, &sec);
		if (ret) {
			fprintf(stderr, "error: failed to load section %d from \"%s\"\n", si, arg_image);
			break;
		}

		// Load section into memory
		ret = mem_load_image(&main_mem, sec.addr, sec.size, infile);
		if (ret) {
			fprintf(stderr, "error: failed to load memory from \"%s\" to address %#"PRIx64"\n",
				arg_image, sec.addr);
			break;
		}
	}

	if (ret == 0) {
		// Sections loaded. Emulate.
		for (int cycles = 0; (max_cycles < 0 || cycles < max_cycles) && ret >= 0 && ret < 256; cycles++) {
			// Check for hit breakpoint on all cores
			int count = 0; // Note: Unused for now
			int hit = 0;
			int corei;
			for (corei = 0; corei < STEM_NUM_CORES; corei++) {
				hit |= bpmap_get(bpmap, cores[corei].pc, &count);
				if (hit) break;
			}
			if (hit) {
				printf("stem: bp hit on core %d at address %#"PRIx64"\n", corei, cores[corei].pc);
				// Present menu to user
				ret = do_menu();
				if (ret != 0) break;
			}

			// Step all cores
			for (corei = 0; corei < STEM_NUM_CORES; corei++) {
				ret = core_step(cores + corei, &main_mem);
			}
		}
		if (ret < 0) {
			fprintf(stderr, "error: an error occurred during emulation\n");
		}
		else if (ret >= 256) { // Core halted
			ret -= 256;
		}

		// Create a hex dump if requested
		if (arg_dump) {
			FILE *dumpfile = fopen(arg_dump, "wb");
			if (!dumpfile) {
				fprintf(stderr, "error: unable to open hex dump file \"%s\"\n", arg_dump);
				ret = 1;
			}
			else {
				int err = mem_dump_hex(&main_mem, 0, 0, dumpfile);
				if (err) {
					ret = err;
				}
				fclose(dumpfile);
			}
		}
	}

	// Clean up
	mem_destroy(&main_mem);
	for (int i = 0; i < STEM_NUM_CORES; i++) {
		core_destroy(cores + i);
	}
	fclose(infile);
	bpmap_delete(bpmap);
	return ret;
}
