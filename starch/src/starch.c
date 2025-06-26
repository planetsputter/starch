// starch.c

#include <string.h>

#include "starch.h"

static const char *names[256] = {
	[op_invalid] = "invalid",

	//
	// Push immediate operations
	//
	[op_push8] = "push8",
	[op_push8u16] = "push8u16",
	[op_push8u32] = "push8u32",
	[op_push8u64] = "push8u64",
	[op_push8i16] = "push8i16",
	[op_push8i32] = "push8i32",
	[op_push8i64] = "push8i64",
	[op_push16] = "push16",
	[op_push16u32] = "push16u32",
	[op_push16u64] = "push16u64",
	[op_push16i32] = "push16i32",
	[op_push16i64] = "push16i64",
	[op_push32] = "push32",
	[op_push32u64] = "push32u64",
	[op_push32i64] = "push32i64",
	[op_push64] = "push64",

	//
	// Pop operations
	//
	[op_pop8] = "pop8",
	[op_pop16] = "pop16",
	[op_pop32] = "pop32",
	[op_pop64] = "pop64",

	//
	// Duplication operations
	//
	[op_dup8] = "dup8",
	[op_dup16] = "dup16",
	[op_dup32] = "dup32",
	[op_dup64] = "dup64",

	//
	// Setting operations
	//
	[op_set8] = "set8",
	[op_set16] = "set16",
	[op_set32] = "set32",
	[op_set64] = "set64",

	//
	// Promotion operations
	//
	[op_prom8u16] = "prom8u16",
	[op_prom8u32] = "prom8u32",
	[op_prom8u64] = "prom8u64",
	[op_prom8i16] = "prom8i16",
	[op_prom8i32] = "prom8i32",
	[op_prom8i64] = "prom8i64",
	[op_prom16u32] = "prom16u32",
	[op_prom16u64] = "prom16u64",
	[op_prom16i32] = "prom16i32",
	[op_prom16i64] = "prom16i64",
	[op_prom32u64] = "prom32u64",
	[op_prom32i64] = "prom32i64",

	//
	// Demotion operations
	//
	[op_dem64to16] = "dem64to16",
	[op_dem64to8] = "dem64to8",
	[op_dem32to8] = "dem32to8",

	//
	// Integer arithmetic operations
	//
	[op_add8] = "add8",
	[op_add16] = "add16",
	[op_add32] = "add32",
	[op_add64] = "add64",
	[op_sub8] = "sub8",
	[op_sub16] = "sub16",
	[op_sub32] = "sub32",
	[op_sub64] = "sub64",
	[op_subr8] = "subr8",
	[op_subr16] = "subr16",
	[op_subr32] = "subr32",
	[op_subr64] = "subr64",
	[op_mulu8] = "mulu8",
	[op_mulu16] = "mulu16",
	[op_mulu32] = "mulu32",
	[op_mulu64] = "mulu64",
	[op_muli8] = "muli8",
	[op_muli16] = "muli16",
	[op_muli32] = "muli32",
	[op_muli64] = "muli64",
	[op_divu8] = "divu8",
	[op_divu16] = "divu16",
	[op_divu32] = "divu32",
	[op_divu64] = "divu64",
	[op_divru8] = "divru8",
	[op_divru16] = "divru16",
	[op_divru32] = "divru32",
	[op_divru64] = "divru64",
	[op_divi8] = "divi8",
	[op_divi16] = "divi16",
	[op_divi32] = "divi32",
	[op_divi64] = "divi64",
	[op_divri8] = "divri8",
	[op_divri16] = "divri16",
	[op_divri32] = "divri32",
	[op_divri64] = "divri64",
	[op_modu8] = "modu8",
	[op_modu16] = "modu16",
	[op_modu32] = "modu32",
	[op_modu64] = "modu64",
	[op_modru8] = "modru8",
	[op_modru16] = "modru16",
	[op_modru32] = "modru32",
	[op_modru64] = "modru64",
	[op_modi8] = "modi8",
	[op_modi16] = "modi16",
	[op_modi32] = "modi32",
	[op_modi64] = "modi64",
	[op_modri8] = "modri8",
	[op_modri16] = "modri16",
	[op_modri32] = "modri32",
	[op_modri64] = "modri64",

	//
	// Bitwise shift operations
	//
	[op_lshift8] = "lshift8",
	[op_lshift16] = "lshift16",
	[op_lshift32] = "lshift32",
	[op_lshift64] = "lshift64",
	[op_rshiftu8] = "rshiftu8",
	[op_rshiftu16] = "rshiftu16",
	[op_rshiftu32] = "rshiftu32",
	[op_rshiftu64] = "rshiftu64",
	[op_rshifti8] = "rshifti8",
	[op_rshifti16] = "rshifti16",
	[op_rshifti32] = "rshifti32",
	[op_rshifti64] = "rshifti64",

	//
	// Bitwise logical operations
	//
	[op_band8] = "band8",
	[op_band16] = "band16",
	[op_band32] = "band32",
	[op_band64] = "band64",
	[op_bor8] = "bor8",
	[op_bor16] = "bor16",
	[op_bor32] = "bor32",
	[op_bor64] = "bor64",
	[op_bxor8] = "bxor8",
	[op_bxor16] = "bxor16",
	[op_bxor32] = "bxor32",
	[op_bxor64] = "bxor64",
	[op_binv8] = "binv8",
	[op_binv16] = "binv16",
	[op_binv32] = "binv32",
	[op_binv64] = "binv64",

	//
	// Boolean logical operations
	//
	[op_land8] = "land8",
	[op_land16] = "land16",
	[op_land32] = "land32",
	[op_land64] = "land64",
	[op_lor8] = "lor8",
	[op_lor16] = "lor16",
	[op_lor32] = "lor32",
	[op_lor64] = "lor64",
	[op_linv8] = "linv8",
	[op_linv16] = "linv16",
	[op_linv32] = "linv32",
	[op_linv64] = "linv64",

	//
	// Comparison operations
	//
	[op_ceq8] = "ceq8",
	[op_ceq16] = "ceq16",
	[op_ceq32] = "ceq32",
	[op_ceq64] = "ceq64",
	[op_cne8] = "cne8",
	[op_cne16] = "cne16",
	[op_cne32] = "cne32",
	[op_cne64] = "cne64",
	[op_cgtu8] = "cgtu8",
	[op_cgtu16] = "cgtu16",
	[op_cgtu32] = "cgtu32",
	[op_cgtu64] = "cgtu64",
	[op_cgti8] = "cgti8",
	[op_cgti16] = "cgti16",
	[op_cgti32] = "cgti32",
	[op_cgti64] = "cgti64",
	[op_cltu8] = "cltu8",
	[op_cltu16] = "cltu16",
	[op_cltu32] = "cltu32",
	[op_cltu64] = "cltu64",
	[op_clti8] = "clti8",
	[op_clti16] = "clti16",
	[op_clti32] = "clti32",
	[op_clti64] = "clti64",
	[op_cgeu8] = "cgeu8",
	[op_cgeu16] = "cgeu16",
	[op_cgeu32] = "cgeu32",
	[op_cgeu64] = "cgeu64",
	[op_cgei8] = "cgei8",
	[op_cgei16] = "cgei16",
	[op_cgei32] = "cgei32",
	[op_cgei64] = "cgei64",
	[op_cleu8] = "cleu8",
	[op_cleu16] = "cleu16",
	[op_cleu32] = "cleu32",
	[op_cleu64] = "cleu64",
	[op_clei8] = "clei8",
	[op_clei16] = "clei16",
	[op_clei32] = "clei32",
	[op_clei64] = "clei64",

	//
	// Function operations
	//
	[op_call] = "call",
	[op_calls] = "calls",
	[op_ret] = "ret",

	//
	// Jump operations
	//
	[op_jmp] = "jmp",
	[op_jmps] = "jmps",
	[op_rjmpi8] = "rjmpi8",
	[op_rjmpi16] = "rjmpi16",
	[op_rjmpi32] = "rjmpi32",

	//
	// Branching operations
	//
	[op_brz8] = "brz8",
	[op_brz16] = "brz16",
	[op_brz32] = "brz32",
	[op_brz64] = "brz64",
	[op_rbrz8i8] = "rbrz8i8",
	[op_rbrz8i16] = "rbrz8i16",
	[op_rbrz8i32] = "rbrz8i32",
	[op_rbrz16i8] = "rbrz16i8",
	[op_rbrz16i16] = "rbrz16i16",
	[op_rbrz16i32] = "rbrz16i32",
	[op_rbrz32i8] = "rbrz32i8",
	[op_rbrz32i16] = "rbrz32i16",
	[op_rbrz32i32] = "rbrz32i32",
	[op_rbrz64i8] = "rbrz64i8",
	[op_rbrz64i16] = "rbrz64i16",
	[op_rbrz64i32] = "rbrz64i32",

	//
	// Memory operations
	//
	[op_load8] = "load8",
	[op_load16] = "load16",
	[op_load32] = "load32",
	[op_load64] = "load64",
	[op_loadpop8] = "loadpop8",
	[op_loadpop16] = "loadpop16",
	[op_loadpop32] = "loadpop32",
	[op_loadpop64] = "loadpop64",
	[op_loadsfp8] = "loadsfp8",
	[op_loadsfp16] = "loadsfp16",
	[op_loadsfp32] = "loadsfp32",
	[op_loadsfp64] = "loadsfp64",
	[op_loadpopsfp8] = "loadpopsfp8",
	[op_loadpopsfp16] = "loadpopsfp16",
	[op_loadpopsfp32] = "loadpopsfp32",
	[op_loadpopsfp64] = "loadpopsfp64",
	[op_store8] = "store8",
	[op_store16] = "store16",
	[op_store32] = "store32",
	[op_store64] = "store64",
	[op_storepop8] = "storepop8",
	[op_storepop16] = "storepop16",
	[op_storepop32] = "storepop32",
	[op_storepop64] = "storepop64",
	[op_storesfp8] = "storesfp8",
	[op_storesfp16] = "storesfp16",
	[op_storesfp32] = "storesfp32",
	[op_storesfp64] = "storesfp64",
	[op_storepopsfp8] = "storepopsfp8",
	[op_storepopsfp16] = "storepopsfp16",
	[op_storepopsfp32] = "storepopsfp32",
	[op_storepopsfp64] = "storepopsfp64",
	[op_storer8] = "storer8",
	[op_storer16] = "storer16",
	[op_storer32] = "storer32",
	[op_storer64] = "storer64",
	[op_storerpop8] = "storerpop8",
	[op_storerpop16] = "storerpop16",
	[op_storerpop32] = "storerpop32",
	[op_storerpop64] = "storerpop64",
	[op_storersfp8] = "storersfp8",
	[op_storersfp16] = "storersfp16",
	[op_storersfp32] = "storersfp32",
	[op_storersfp64] = "storersfp64",
	[op_storerpopsfp8] = "storerpopsfp8",
	[op_storerpopsfp16] = "storerpopsfp16",
	[op_storerpopsfp32] = "storerpopsfp32",
	[op_storerpopsfp64] = "storerpopsfp64",

	//
	// Special Operations
	//
	[op_setsbp] = "setsbp",
	[op_setsfp] = "setsfp",
	[op_setsp] = "setsp",
	[op_setslp] = "setslp",
	[op_halt] = "halt",
	[op_ext] = "ext",
	[op_nop] = "nop",
};

const char *name_for_opcode(uint8_t opcode)
{
	return names[opcode];
}

int opcode_for_name(const char *name)
{
	// @todo: would be more efficient to have a list sorted by name and do a binary search
	for (size_t i = 0; i < sizeof(names) / sizeof(*names); i++) {
		if (names[i] && strcmp(name, names[i]) == 0) return i;
	}
	return -1;
}

int imm_count_for_opcode(uint8_t opcode)
{
	int ret = 0;

	switch (opcode) {
	case op_invalid:
		break;

	//
	// Push immediate operations
	//
	case op_push8:
	case op_push8u16:
	case op_push8u32:
	case op_push8u64:
	case op_push8i16:
	case op_push8i32:
	case op_push8i64:
	case op_push16:
	case op_push16u32:
	case op_push16u64:
	case op_push16i32:
	case op_push16i64:
	case op_push32:
	case op_push32u64:
	case op_push32i64:
	case op_push64:
		ret = 1;
		break;

	//
	// Pop operations
	//
	case op_pop8:
	case op_pop16:
	case op_pop32:
	case op_pop64:
		break;

	//
	// Duplication operations
	//
	case op_dup8:
	case op_dup16:
	case op_dup32:
	case op_dup64:
		break;

	//
	// Setting operations
	//
	case op_set8:
	case op_set16:
	case op_set32:
	case op_set64:
		break;

	//
	// Promotion Operations
	//
	case op_prom8u16:
	case op_prom8u32:
	case op_prom8u64:
	case op_prom8i16:
	case op_prom8i32:
	case op_prom8i64:
	case op_prom16u32:
	case op_prom16u64:
	case op_prom16i32:
	case op_prom16i64:
	case op_prom32u64:
	case op_prom32i64:
		break;

	//
	// Demotion Operations
	//
	case op_dem64to16:
	case op_dem64to8:
	case op_dem32to8:
		break;

	//
	// Integer Arithmetic Operations
	//
	case op_add8:
	case op_add16:
	case op_add32:
	case op_add64:
	case op_sub8:
	case op_sub16:
	case op_sub32:
	case op_sub64:
	case op_subr8:
	case op_subr16:
	case op_subr32:
	case op_subr64:
	case op_mulu8:
	case op_mulu16:
	case op_mulu32:
	case op_mulu64:
	case op_muli8:
	case op_muli16:
	case op_muli32:
	case op_muli64:
	case op_divu8:
	case op_divu16:
	case op_divu32:
	case op_divu64:
	case op_divru8:
	case op_divru16:
	case op_divru32:
	case op_divru64:
	case op_divi8:
	case op_divi16:
	case op_divi32:
	case op_divi64:
	case op_divri8:
	case op_divri16:
	case op_divri32:
	case op_divri64:
	case op_modu8:
	case op_modu16:
	case op_modu32:
	case op_modu64:
	case op_modru8:
	case op_modru16:
	case op_modru32:
	case op_modru64:
	case op_modi8:
	case op_modi16:
	case op_modi32:
	case op_modi64:
	case op_modri8:
	case op_modri16:
	case op_modri32:
	case op_modri64:
		break;

	//
	// Bitwise shift operations
	//
	case op_lshift8:
	case op_lshift16:
	case op_lshift32:
	case op_lshift64:
	case op_rshiftu8:
	case op_rshiftu16:
	case op_rshiftu32:
	case op_rshiftu64:
	case op_rshifti8:
	case op_rshifti16:
	case op_rshifti32:
	case op_rshifti64:
		break;

	//
	// Bitwise logical operations
	//
	case op_band8:
	case op_band16:
	case op_band32:
	case op_band64:
	case op_bor8:
	case op_bor16:
	case op_bor32:
	case op_bor64:
	case op_bxor8:
	case op_bxor16:
	case op_bxor32:
	case op_bxor64:
	case op_binv8:
	case op_binv16:
	case op_binv32:
	case op_binv64:
		break;

	//
	// Boolean logical operations
	//
	case op_land8:
	case op_land16:
	case op_land32:
	case op_land64:
	case op_lor8:
	case op_lor16:
	case op_lor32:
	case op_lor64:
	case op_linv8:
	case op_linv16:
	case op_linv32:
	case op_linv64:
		break;

	//
	// Comparison operations
	//
	case op_ceq8:
	case op_ceq16:
	case op_ceq32:
	case op_ceq64:
	case op_cne8:
	case op_cne16:
	case op_cne32:
	case op_cne64:
	case op_cgtu8:
	case op_cgtu16:
	case op_cgtu32:
	case op_cgtu64:
	case op_cgti8:
	case op_cgti16:
	case op_cgti32:
	case op_cgti64:
	case op_cltu8:
	case op_cltu16:
	case op_cltu32:
	case op_cltu64:
	case op_clti8:
	case op_clti16:
	case op_clti32:
	case op_clti64:
	case op_cgeu8:
	case op_cgeu16:
	case op_cgeu32:
	case op_cgeu64:
	case op_cgei8:
	case op_cgei16:
	case op_cgei32:
	case op_cgei64:
	case op_cleu8:
	case op_cleu16:
	case op_cleu32:
	case op_cleu64:
	case op_clei8:
	case op_clei16:
	case op_clei32:
	case op_clei64:
		break;

	//
	// Function operations
	//
	case op_call:
		ret = 1;
		break;
	case op_calls:
	case op_ret:
		break;

	//
	// Jump operations
	//
	case op_jmp:
		ret = 1;
		break;
	case op_jmps:
		break;
	case op_rjmpi8:
	case op_rjmpi16:
	case op_rjmpi32:
		ret = 1;
		break;

	//
	// Branching operations
	//
	case op_brz8:
	case op_brz16:
	case op_brz32:
	case op_brz64:
	case op_rbrz8i8:
	case op_rbrz8i16:
	case op_rbrz8i32:
	case op_rbrz16i8:
	case op_rbrz16i16:
	case op_rbrz16i32:
	case op_rbrz32i8:
	case op_rbrz32i16:
	case op_rbrz32i32:
	case op_rbrz64i8:
	case op_rbrz64i16:
	case op_rbrz64i32:
		ret = 1;
		break;

	//
	// Memory operations
	//
	case op_load8:
	case op_load16:
	case op_load32:
	case op_load64:
	case op_loadpop8:
	case op_loadpop16:
	case op_loadpop32:
	case op_loadpop64:
	case op_loadsfp8:
	case op_loadsfp16:
	case op_loadsfp32:
	case op_loadsfp64:
	case op_loadpopsfp8:
	case op_loadpopsfp16:
	case op_loadpopsfp32:
	case op_loadpopsfp64:
	case op_store8:
	case op_store16:
	case op_store32:
	case op_store64:
	case op_storepop8:
	case op_storepop16:
	case op_storepop32:
	case op_storepop64:
	case op_storesfp8:
	case op_storesfp16:
	case op_storesfp32:
	case op_storesfp64:
	case op_storepopsfp8:
	case op_storepopsfp16:
	case op_storepopsfp32:
	case op_storepopsfp64:
	case op_storer8:
	case op_storer16:
	case op_storer32:
	case op_storer64:
	case op_storerpop8:
	case op_storerpop16:
	case op_storerpop32:
	case op_storerpop64:
	case op_storersfp8:
	case op_storersfp16:
	case op_storersfp32:
	case op_storersfp64:
	case op_storerpopsfp8:
	case op_storerpopsfp16:
	case op_storerpopsfp32:
	case op_storerpopsfp64:
		break;

	//
	// Special Operations
	//
	case op_setsbp:
	case op_setsfp:
	case op_setsp:
	case op_setslp:
		ret = 1;
		break;
	case op_halt:
	case op_ext:
		// @todo
	case op_nop:
		break;

	default:
		ret = -1;
		break;
	}

	return ret;
}

void min_max_for_dt(int dt, long long *min, long long *max)
{
	// Set minimum
	if (min) switch (dt) {
	case dt_a8:
	case dt_i8:
		*min = -0x80;
		break;
	case dt_a16:
	case dt_i16:
		*min = -0x8000;
		break;
	case dt_a32:
	case dt_i32:
		*min = -0x80000000l;
		break;
	case dt_a64:
	case dt_i64:
		*min = -0x8000000000000000l;
		break;
	case dt_u8:
	case dt_u16:
	case dt_u32:
	case dt_u64:
		*min = 0;
		break;
	}

	// Set maximum
	if (max) switch (dt) {
	case dt_a8:
	case dt_u8:
		*max = 0xff;
		break;
	case dt_i8:
		*max = 0x7f;
		break;
	case dt_a16:
	case dt_u16:
		*max = 0xffff;
		break;
	case dt_i16:
		*max = 0x7fff;
		break;
	case dt_a32:
	case dt_u32:
		*max = 0xffffffffl;
		break;
	case dt_i32:
		*max = 0x7fffffffl;
		break;
	case dt_a64:
	case dt_u64:
		*max = 0xfffffffffffffffflu;
		break;
	case dt_i64:
		*max = 0x7ffffffffffffffflu;
		break;
	}
}

int size_for_dt(int dt)
{
	switch (dt) {
	case dt_a8:
	case dt_u8:
	case dt_i8:
		return 1;
	case dt_a16:
	case dt_u16:
	case dt_i16:
		return 2;
	case dt_a32:
	case dt_u32:
	case dt_i32:
		return 4;
	case dt_a64:
	case dt_u64:
	case dt_i64:
		return 8;
	}
	return 0;
}

int imm_types_for_opcode(uint8_t opcode, int *dts)
{
	int ret = 0;

	switch (opcode) {
	//
	// Push immediate operations
	//
	case op_push8:     *dts = dt_a8; break;
	case op_push8u16:  *dts = dt_u8; break;
	case op_push8u32:  *dts = dt_u8; break;
	case op_push8u64:  *dts = dt_u8; break;
	case op_push8i16:  *dts = dt_i8; break;
	case op_push8i32:  *dts = dt_i8; break;
	case op_push8i64:  *dts = dt_i8; break;
	case op_push16:    *dts = dt_a16; break;
	case op_push16u32: *dts = dt_u16; break;
	case op_push16u64: *dts = dt_u16; break;
	case op_push16i32: *dts = dt_i16; break;
	case op_push16i64: *dts = dt_i16; break;
	case op_push32:    *dts = dt_a32; break;
	case op_push32u64: *dts = dt_u32; break;
	case op_push32i64: *dts = dt_i32; break;
	case op_push64:    *dts = dt_a64; break;

	//
	// Pop operations
	//
	case op_pop8:
	case op_pop16:
	case op_pop32:
	case op_pop64:
		break;

	//
	// Duplication operations
	//
	case op_dup8:
	case op_dup16:
	case op_dup32:
	case op_dup64:
		break;

	//
	// Setting operations
	//
	case op_set8:
	case op_set16:
	case op_set32:
	case op_set64:
		break;

	//
	// Promotion Operations
	//
	case op_prom8u16:
	case op_prom8u32:
	case op_prom8u64:
	case op_prom8i16:
	case op_prom8i32:
	case op_prom8i64:
	case op_prom16u32:
	case op_prom16u64:
	case op_prom16i32:
	case op_prom16i64:
	case op_prom32u64:
	case op_prom32i64:
		break;

	//
	// Demotion Operations
	//
	case op_dem64to16:
	case op_dem64to8:
	case op_dem32to8:
		break;

	//
	// Integer Arithmetic Operations
	//
	case op_add8:
	case op_add16:
	case op_add32:
	case op_add64:
	case op_sub8:
	case op_sub16:
	case op_sub32:
	case op_sub64:
	case op_subr8:
	case op_subr16:
	case op_subr32:
	case op_subr64:
	case op_mulu8:
	case op_mulu16:
	case op_mulu32:
	case op_mulu64:
	case op_muli8:
	case op_muli16:
	case op_muli32:
	case op_muli64:
	case op_divu8:
	case op_divu16:
	case op_divu32:
	case op_divu64:
	case op_divru8:
	case op_divru16:
	case op_divru32:
	case op_divru64:
	case op_divi8:
	case op_divi16:
	case op_divi32:
	case op_divi64:
	case op_divri8:
	case op_divri16:
	case op_divri32:
	case op_divri64:
	case op_modu8:
	case op_modu16:
	case op_modu32:
	case op_modu64:
	case op_modru8:
	case op_modru16:
	case op_modru32:
	case op_modru64:
	case op_modi8:
	case op_modi16:
	case op_modi32:
	case op_modi64:
	case op_modri8:
	case op_modri16:
	case op_modri32:
	case op_modri64:
		break;

	//
	// Bitwise shift operations
	//
	case op_lshift8:
	case op_lshift16:
	case op_lshift32:
	case op_lshift64:
	case op_rshiftu8:
	case op_rshiftu16:
	case op_rshiftu32:
	case op_rshiftu64:
	case op_rshifti8:
	case op_rshifti16:
	case op_rshifti32:
	case op_rshifti64:
		break;

	//
	// Bitwise logical operations
	//
	case op_band8:
	case op_band16:
	case op_band32:
	case op_band64:
	case op_bor8:
	case op_bor16:
	case op_bor32:
	case op_bor64:
	case op_bxor8:
	case op_bxor16:
	case op_bxor32:
	case op_bxor64:
	case op_binv8:
	case op_binv16:
	case op_binv32:
	case op_binv64:
		break;

	//
	// Boolean logical operations
	//
	case op_land8:
	case op_land16:
	case op_land32:
	case op_land64:
	case op_lor8:
	case op_lor16:
	case op_lor32:
	case op_lor64:
	case op_linv8:
	case op_linv16:
	case op_linv32:
	case op_linv64:
		break;

	//
	// Comparison operations
	//
	case op_ceq8:
	case op_ceq16:
	case op_ceq32:
	case op_ceq64:
	case op_cne8:
	case op_cne16:
	case op_cne32:
	case op_cne64:
	case op_cgtu8:
	case op_cgtu16:
	case op_cgtu32:
	case op_cgtu64:
	case op_cgti8:
	case op_cgti16:
	case op_cgti32:
	case op_cgti64:
	case op_cltu8:
	case op_cltu16:
	case op_cltu32:
	case op_cltu64:
	case op_clti8:
	case op_clti16:
	case op_clti32:
	case op_clti64:
	case op_cgeu8:
	case op_cgeu16:
	case op_cgeu32:
	case op_cgeu64:
	case op_cgei8:
	case op_cgei16:
	case op_cgei32:
	case op_cgei64:
	case op_cleu8:
	case op_cleu16:
	case op_cleu32:
	case op_cleu64:
	case op_clei8:
	case op_clei16:
	case op_clei32:
	case op_clei64:
		break;

	//
	// Function operations
	//
	case op_call:
		*dts = dt_u64;
		break;
	case op_calls:
	case op_ret:
		break;

	//
	// Jump operations
	//
	case op_jmp: *dts = dt_u64; break;
	case op_jmps: break;
	case op_rjmpi8:  *dts = dt_i8; break;
	case op_rjmpi16: *dts = dt_i16; break;
	case op_rjmpi32: *dts = dt_i32; break;

	//
	// Branching operations
	//
	case op_brz8:
	case op_brz16:
	case op_brz32:
	case op_brz64:
		*dts = dt_u64;
		break;
	case op_rbrz8i8:
	case op_rbrz16i8:
	case op_rbrz32i8:
	case op_rbrz64i8:
		*dts = dt_i8;
		break;
	case op_rbrz8i16:
	case op_rbrz16i16:
	case op_rbrz32i16:
	case op_rbrz64i16:
		*dts = dt_i16;
		break;
	case op_rbrz8i32:
	case op_rbrz16i32:
	case op_rbrz32i32:
	case op_rbrz64i32:
		*dts = dt_i32;
		break;

	//
	// Memory operations
	//
	case op_load8:
	case op_load16:
	case op_load32:
	case op_load64:
	case op_loadpop8:
	case op_loadpop16:
	case op_loadpop32:
	case op_loadpop64:
	case op_loadsfp8:
	case op_loadsfp16:
	case op_loadsfp32:
	case op_loadsfp64:
	case op_loadpopsfp8:
	case op_loadpopsfp16:
	case op_loadpopsfp32:
	case op_loadpopsfp64:
	case op_store8:
	case op_store16:
	case op_store32:
	case op_store64:
	case op_storepop8:
	case op_storepop16:
	case op_storepop32:
	case op_storepop64:
	case op_storesfp8:
	case op_storesfp16:
	case op_storesfp32:
	case op_storesfp64:
	case op_storepopsfp8:
	case op_storepopsfp16:
	case op_storepopsfp32:
	case op_storepopsfp64:
	case op_storer8:
	case op_storer16:
	case op_storer32:
	case op_storer64:
	case op_storerpop8:
	case op_storerpop16:
	case op_storerpop32:
	case op_storerpop64:
	case op_storersfp8:
	case op_storersfp16:
	case op_storersfp32:
	case op_storersfp64:
	case op_storerpopsfp8:
	case op_storerpopsfp16:
	case op_storerpopsfp32:
	case op_storerpopsfp64:
		break;

	//
	// Special Operations
	//
	case op_setsbp:
	case op_setsfp:
	case op_setsp:
	case op_setslp:
		*dts = dt_u64;
		break;
	case op_halt:
	case op_ext:
		// @todo
	case op_nop:
		break;

	default:
		ret = 1;
		break;
	}

	return ret;
}

const char *name_for_sterr(int sterr)
{
	switch (sterr) {
	// Generic starch errors
	case STERR_NONE:
		return "STERR_NONE";
	case STERR_BAD_INST:
		return "STERR_BAD_INST";
	case STERR_BAD_ALIGN:
		return "STERR_BAD_ALIGN";
	case STERR_BAD_IO_ACCESS:
		return "STERR_BAD_IO_ACCESS";
	case STERR_BAD_FRAME_ACCESS:
		return "STERR_BAD_FRAME_ACCESS";
	case STERR_BAD_STACK_ACCESS:
		return "STERR_BAD_STACK_ACCESS";
	case STERR_BAD_ADDR:
		return "STERR_BAD_ADDR";
	case STERR_HALT:
		return "STERR_HALT";
	}
	return "STERR_UNKNOWN";
}
