// distasm.c

#include "starch.h"
#include "carg.h"
#include "stub.h"

// Variables set by command-line arguments
const char *arg_help = NULL;
const char *arg_bin = NULL;
const char *arg_output = NULL;

struct carg_desc arg_descs[] = {
	{
		CARG_TYPE_UNARY, // Type
		'h',            // Flag
		"--help",       // Name
		&arg_help,      // Value
		false,          // Required
		"show usage",   // Usage text
		NULL            // Value hint
	},
	{
		CARG_TYPE_NAMED,  // Type
		'o',             // Flag
		"--output",      // Name
		&arg_output,     // Value
		false,           // Required
		"binary output", // Usage text
		"output"         // Value hint
	},
	{
		CARG_TYPE_POSITIONAL, // Type
		'\0',                // Flag
		NULL,                // Name
		&arg_bin,            // Value
		true,                // Required
		"starch binary",     // Usage text
		"binary"             // Value hint
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
			fprintf(stderr, "warning: Only printing usage. Other arguments present.\n");
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

	// Open input binary
	FILE *infile = fopen(arg_bin, "rb");
	if (!infile) {
		fprintf(stderr, "error: failed to open \"%s\"\n", arg_bin);
		return 1;
	}
	// Verify the input file is a valid stub file
	int ret = stub_verify(infile);
	if (ret) {
		fclose(infile);
		fprintf(stderr, "error: \"%s\" is not a valid stub file\n", arg_bin);
		return ret;
	}
	// Load the first section, seeking to the beginning of the section data
	struct stub_sec sec;
	ret = stub_load_section(infile, 0, &sec);
	if (ret) {
		fclose(infile);
		fprintf(stderr, "error: failed to load section 0 from \"%s\"\n", arg_bin);
		return ret;
	}

	// Open output file
	FILE *outfile;
	if (arg_output) {
		outfile = fopen(arg_output, "w");
		if (!outfile) {
			fclose(infile);
			fprintf(stderr, "error: failed to open %s\n", arg_output);
			return 1;
		}
	}
	else {
		outfile = stdout;
	}

	// Read in input binary
	// @todo: limit reads to section 0 size
	int opcode, num_imm;
	ret = 0;
	while ((opcode = fgetc(infile)) != EOF) {
		// Print opcode name
		ret = fprintf(outfile, "%s", name_for_opcode(opcode));
		if (ret < 0) {
			fprintf(stderr, "error: failed to write to %s\n", arg_output ? arg_output : "stdout");
			ret = 1;
			break;
		}
		ret = 0;

		// Determine number of opcode arguments
		num_imm = imm_count_for_opcode(opcode);
		if (num_imm < 0 || num_imm > 1) { // @todo: handle more than one immediate
			fprintf(stderr, "error: invalid number (%d) of immediates for opcode 0x%02x\n",
				num_imm, opcode);
			ret = 1;
			break;
		}

		// Print opcode arguments
		for (int i = 0; i < num_imm; i++) {
			int dt;
			ret = imm_types_for_opcode(opcode, &dt);
			if (ret < 0) {
				fprintf(stderr, "error: unable to determine immediate type for opcode 0x%02x\n",
					opcode);
				ret = 1;
				break;
			}
			ret = 0;

			int imm_len = size_for_dt(dt);

			// Read little-endian immediate value
			int64_t val = 0;
			int b;
			for (int j = 0; j < imm_len; j++) {
				b = fgetc(infile);
				if (b == EOF) {
					fprintf(stderr, "error: failed to read from %s\n", arg_bin);
					ret = 1;
					break;
				}
				val |= b << (j * 8);
			}
			if (ret) break;

			// Print value
			switch (dt) {
			case dt_a8:
			case dt_u8:
				ret = fprintf(outfile, " %hhu", (uint8_t)val);
				break;
			case dt_a16:
			case dt_u16:
				ret = fprintf(outfile, " %hu", (uint16_t)val);
				break;
			case dt_a32:
			case dt_u32:
				ret = fprintf(outfile, " %u", (uint32_t)val);
				break;
			case dt_a64:
			case dt_u64:
				ret = fprintf(outfile, " %lu", (uint64_t)val);
				break;
			case dt_i8:
				ret = fprintf(outfile, " %d", (int8_t)val);
				break;
			case dt_i16:
				ret = fprintf(outfile, " %d", (int16_t)val);
				break;
			case dt_i32:
				ret = fprintf(outfile, " %d", (int32_t)val);
				break;
			case dt_i64:
				ret = fprintf(outfile, " %ld", val);
				break;
			}
			if (ret < 0) {
				fprintf(stderr, "error: failed to write to %s\n", arg_output ? arg_output : "stdout");
				ret = 1;
				break;
			}
			ret = 0;
		}
		if (ret) break;
		fprintf(outfile, "\n");
	}
	// Check for error
	if (ferror(outfile) || ferror(infile)) {
		ret = 1;
	}

	// Close file handles
	fclose(infile);
	if (outfile != stdout) {
		fclose(outfile);
	}

	return ret;
}
