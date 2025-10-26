// distasm.c

#include <inttypes.h>

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
void detect_non_help_arg(struct carg_desc *desc, const char *arg)
{
	(void)arg;
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

	// Get number of sections
	int maxnsec = 0, nsec = 0;
	ret = stub_get_section_counts(infile, &maxnsec, &nsec);
	if (ret) {
		fprintf(stderr, "error: failed to get section counts from \"%s\"\n", arg_bin);
		return ret;
	}

	// Open output file
	FILE *outfile;
	if (arg_output) {
		outfile = fopen(arg_output, "w");
		if (!outfile) {
			fclose(infile);
			fprintf(stderr, "error: failed to open \"%s\"\n", arg_output);
			return 1;
		}
	}
	else {
		outfile = stdout;
	}

	// Iterate through each section of the input file
	struct stub_sec sec;
	for (int si = 0; ret == 0 && si < nsec; si++) {
		// Load each section, seeking to the beginning of the section data
		ret = stub_load_section(infile, si, &sec);
		if (ret) {
			fclose(infile);
			fprintf(stderr, "error: failed to load section %d from \"%s\"\n", si, arg_bin);
			break;
		}

		// Print section description
		ret = fprintf(outfile, ".section %#"PRIx64"\n", sec.addr);
		if (ret < 0) {
			fprintf(stderr, "error: failed to write to \"%s\"\n", arg_output ? arg_output : "stdout");
			ret = 1;
			break;
		}

		// Disassemble all bytes in section
		for (uint64_t di = 0; di < sec.size; ) {
			int opcode = fgetc(infile);
			if (opcode == EOF) {
				fprintf(stderr, "error: unexpected EOF in \"%s\"\n", arg_bin);
				ret = 1;
				break;
			}
			di++;

			// Look up opcode name
			const char *name = name_for_opcode(opcode);
			if (name == NULL) {
				fprintf(stderr, "error: failed to look up name for opcode %02x\n", opcode);
				ret = 1;
				break;
			}

			// Determine type of immediate value
			int sdt = imm_type_for_opcode(opcode);
			if (sdt < 0) {
				fprintf(stderr, "error: unable to determine immediate type for opcode 0x%02x\n",
					opcode);
				ret = 1;
				break;
			}

			int imm_len = sdt_size(sdt);

			// Read little-endian immediate value
			int64_t val = 0;
			int64_t b;
			ret = 0;
			for (int j = 0; j < imm_len; j++) {
				b = fgetc(infile);
				if (b == EOF) {
					fprintf(stderr, "error: unexpected EOF in \"%s\"\n", arg_bin);
					ret = 1;
					break;
				}
				di++;
				val |= b << (j * 8);
			}
			if (ret) break;

			// Print opcode name
			ret = fprintf(outfile, "%s", name);
			if (ret < 0) {
				fprintf(stderr, "error: failed to write to \"%s\"\n", arg_output ? arg_output : "stdout");
				ret = 1;
				break;
			}

			// Print value
			if (sdt == SDT_VOID) {
				// Do nothing
			}
			else if (val == 0) { // Print zero as "0", no matter the data type
				ret = fprintf(outfile, " 0");
			}
			else switch (sdt) {
			case SDT_A8:
			case SDT_U8:
			case SDT_A16:
			case SDT_U16:
			case SDT_A32:
			case SDT_U32:
			case SDT_A64:
			case SDT_U64:
				ret = fprintf(outfile, " %#"PRIx64, (uint64_t)val);
				break;
			case SDT_I8:
				val = (int8_t)val;
				ret = fprintf(outfile, " %s%#"PRIx64, val < 0 ? "-" : "", val < 0 ? -val : val);
				break;
			case SDT_I16:
				val = (int16_t)val;
				ret = fprintf(outfile, " %s%#"PRIx64, val < 0 ? "-" : "", val < 0 ? -val : val);
				break;
			case SDT_I32:
				val = (int32_t)val;
				ret = fprintf(outfile, " %s%#"PRIx64, val < 0 ? "-" : "", val < 0 ? -val : val);
				break;
			case SDT_I64:
				ret = fprintf(outfile, " %s%#"PRIx64, val < 0 ? "-" : "", val < 0 ? -val : val);
				break;
			}
			if (ret >= 0) {
				ret = fprintf(outfile, "\n");
			}
			if (ret < 0) {
				fprintf(stderr, "error: failed to write to \"%s\"\n", arg_output ? arg_output : "stdout");
				ret = 1;
				break;
			}
			ret = 0;
		}
	}

	// Close file handles
	fclose(infile);
	if (outfile != stdout) {
		fclose(outfile);
	}

	return ret;
}
