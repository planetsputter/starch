// starch.c

#include <string.h>

#include "starch.h"

static const char *names[256] = {
	//
	// Push immediate operations
	//
	[op_invalid] = "invalid",
	[op_push1] = "push1",
	[op_push1u2] = "push1u2",
	[op_push1u4] = "push1u4",
	[op_push1u8] = "push1u8",
	[op_push1i2] = "push1i2",
	[op_push1i4] = "push1i4",
	[op_push1i8] = "push1i8",
	[op_push2] = "push2",
	[op_push2u4] = "push2u4",
	[op_push2u8] = "push2u8",
	[op_push2i4] = "push2i4",
	[op_push2i8] = "push2i8",
	[op_push4] = "push4",
	[op_push4u8] = "push4u8",
	[op_push4i8] = "push4i8",
	[op_push8] = "push8",

	//
	// Pop operations
	//
	[op_pop1] = "pop1",
	[op_pop2] = "pop2",
	[op_pop4] = "pop4",
	[op_pop8] = "pop8",
	[op_pop] = "pop",
	[op_dup1] = "dup1",
	[op_dup2] = "dup2",
	[op_dup4] = "dup4",
	[op_dup8] = "dup8",
	[op_set1] = "set1",
	[op_set2] = "set2",
	[op_set4] = "set4",
	[op_set8] = "set8",
	[op_prom1u2] = "prom1u2",
	[op_prom1u4] = "prom1u4",
	[op_prom1u8] = "prom1u8",
	[op_prom1i2] = "prom1i2",
	[op_prom1i4] = "prom1i4",
	[op_prom1i8] = "prom1i8",
	[op_prom2u4] = "prom2u4",
	[op_prom2u8] = "prom2u8",
	[op_prom2i4] = "prom2i4",
	[op_prom2i8] = "prom2i8",
	[op_prom4u8] = "prom4u8",
	[op_prom4i8] = "prom4i8",
	[op_dem8_4] = "dem8_4",
	[op_dem8_2] = "dem8_2",
	[op_dem8_1] = "dem8_1",
	[op_dem4_2] = "dem4_2",
	[op_dem4_1] = "dem4_1",
	[op_dem2_1] = "dem2_1",
	[op_add1] = "add1",
	[op_add2] = "add2",
	[op_add4] = "add4",
	[op_add8] = "add8",
	[op_sub1] = "sub1",
	[op_sub2] = "sub2",
	[op_sub4] = "sub4",
	[op_sub8] = "sub8",
	[op_mulu1] = "mulu1",
	[op_mulu2] = "mulu2",
	[op_mulu4] = "mulu4",
	[op_mulu8] = "mulu8",
	[op_muli1] = "muli1",
	[op_muli2] = "muli2",
	[op_muli4] = "muli4",
	[op_muli8] = "muli8",
	[op_divu1] = "divu1",
	[op_divu2] = "divu2",
	[op_divu4] = "divu4",
	[op_divu8] = "divu8",
	[op_divi1] = "divi1",
	[op_divi2] = "divi2",
	[op_divi4] = "divi4",
	[op_divi8] = "divi8",
	[op_modu1] = "modu1",
	[op_modu2] = "modu2",
	[op_modu4] = "modu4",
	[op_modu8] = "modu8",
	[op_modi1] = "modi1",
	[op_modi2] = "modi2",
	[op_modi4] = "modi4",
	[op_modi8] = "modi8",
	[op_lshift1] = "lshift1",
	[op_lshift2] = "lshift2",
	[op_lshift4] = "lshift4",
	[op_lshift8] = "lshift8",
	[op_rshiftu1] = "rshiftu1",
	[op_rshiftu2] = "rshiftu2",
	[op_rshiftu4] = "rshiftu4",
	[op_rshiftu8] = "rshiftu8",
	[op_rshifti1] = "rshifti1",
	[op_rshifti2] = "rshifti2",
	[op_rshifti4] = "rshifti4",
	[op_rshifti8] = "rshifti8",
	[op_band1] = "band1",
	[op_band2] = "band2",
	[op_band4] = "band4",
	[op_band8] = "band8",
	[op_bor1] = "bor1",
	[op_bor2] = "bor2",
	[op_bor4] = "bor4",
	[op_bor8] = "bor8",
	[op_bxor1] = "bxor1",
	[op_bxor2] = "bxor2",
	[op_bxor4] = "bxor4",
	[op_bxor8] = "bxor8",
	[op_binv1] = "binv1",
	[op_binv2] = "binv2",
	[op_binv4] = "binv4",
	[op_binv8] = "binv8",
	[op_land1] = "land1",
	[op_land2] = "land2",
	[op_land4] = "land4",
	[op_land8] = "land8",
	[op_lor1] = "lor1",
	[op_lor2] = "lor2",
	[op_lor4] = "lor4",
	[op_lor8] = "lor8",
	[op_linv1] = "linv1",
	[op_linv2] = "linv2",
	[op_linv4] = "linv4",
	[op_linv8] = "linv8",
	[op_ceq1] = "ceq1",
	[op_ceq2] = "ceq2",
	[op_ceq4] = "ceq4",
	[op_ceq8] = "ceq8",
	[op_cne1] = "cne1",
	[op_cne2] = "cne2",
	[op_cne4] = "cne4",
	[op_cne8] = "cne8",
	[op_cgtu1] = "cgtu1",
	[op_cgtu2] = "cgtu2",
	[op_cgtu4] = "cgtu4",
	[op_cgtu8] = "cgtu8",
	[op_cgti1] = "cgti1",
	[op_cgti2] = "cgti2",
	[op_cgti4] = "cgti4",
	[op_cgti8] = "cgti8",
	[op_cltu1] = "cltu1",
	[op_cltu2] = "cltu2",
	[op_cltu4] = "cltu4",
	[op_cltu8] = "cltu8",
	[op_clti1] = "clti1",
	[op_clti2] = "clti2",
	[op_clti4] = "clti4",
	[op_clti8] = "clti8",
	[op_cgeu1] = "cgeu1",
	[op_cgeu2] = "cgeu2",
	[op_cgeu4] = "cgeu4",
	[op_cgeu8] = "cgeu8",
	[op_cgei1] = "cgei1",
	[op_cgei2] = "cgei2",
	[op_cgei4] = "cgei4",
	[op_cgei8] = "cgei8",
	[op_cleu1] = "cleu1",
	[op_cleu2] = "cleu2",
	[op_cleu4] = "cleu4",
	[op_cleu8] = "cleu8",
	[op_clei1] = "clei1",
	[op_clei2] = "clei2",
	[op_clei4] = "clei4",
	[op_clei8] = "clei8",
	[op_call] = "call",
	[op_calls] = "calls",
	[op_ret] = "ret",
	[op_jmp] = "jmp",
	[op_jmps] = "jmps",
	[op_rjmpi1] = "rjmpi1",
	[op_rjmpi2] = "rjmpi2",
	[op_rjmpi4] = "rjmpi4",
	[op_rjmpi8] = "rjmpi8",
	[op_brz1] = "brz1",
	[op_brz2] = "brz2",
	[op_brz4] = "brz4",
	[op_brz8] = "brz8",
	[op_rbrz1i1] = "op_rbrz1i1",
	[op_rbrz2i1] = "op_rbrz2i1",
	[op_rbrz4i1] = "op_rbrz4i1",
	[op_rbrz8i1] = "op_rbrz8i1",
	[op_rbrz1i2] = "op_rbrz1i2",
	[op_rbrz2i2] = "op_rbrz2i2",
	[op_rbrz4i2] = "op_rbrz4i2",
	[op_rbrz8i2] = "op_rbrz8i2",
	[op_rbrz1i4] = "op_rbrz1i4",
	[op_rbrz2i4] = "op_rbrz2i4",
	[op_rbrz4i4] = "op_rbrz4i4",
	[op_rbrz8i4] = "op_rbrz8i4",
	[op_rbrz1i8] = "op_rbrz1i8",
	[op_rbrz2i8] = "op_rbrz2i8",
	[op_rbrz4i8] = "op_rbrz4i8",
	[op_rbrz8i8] = "op_rbrz8i8",
	[op_load1] = "load1",
	[op_load2] = "load2",
	[op_load4] = "load4",
	[op_load8] = "load8",
	[op_loadpop1] = "loadpop1",
	[op_loadpop2] = "loadpop2",
	[op_loadpop4] = "loadpop4",
	[op_loadpop8] = "loadpop8",
	[op_loadsfp1] = "loadsfp1",
	[op_loadsfp2] = "loadsfp2",
	[op_loadsfp4] = "loadsfp4",
	[op_loadsfp8] = "loadsfp8",
	[op_loadpopsfp1] = "loadpopsfp1",
	[op_loadpopsfp2] = "loadpopsfp2",
	[op_loadpopsfp4] = "loadpopsfp4",
	[op_loadpopsfp8] = "loadpopsfp8",
	[op_store1] = "store1",
	[op_store2] = "store2",
	[op_store4] = "store4",
	[op_store8] = "store8",
	[op_storepop1] = "storepop1",
	[op_storepop2] = "storepop2",
	[op_storepop4] = "storepop4",
	[op_storepop8] = "storepop8",
	[op_storesfp1] = "storesfp1",
	[op_storesfp2] = "storesfp2",
	[op_storesfp4] = "storesfp4",
	[op_storesfp8] = "storesfp8",
	[op_storepopsfp1] = "storepopsfp1",
	[op_storepopsfp2] = "storepopsfp2",
	[op_storepopsfp4] = "storepopsfp4",
	[op_storepopsfp8] = "storepopsfp8",
	[op_admin] = "admin",
	[op_ext] = "ext",
	[op_nop] = "nop",
};

const char *name_for_opcode(int opcode)
{
	return ((size_t)opcode < sizeof(names) / sizeof(*names)) ? names[opcode] : NULL;
}

int opcode_for_name(const char *name)
{
	// @todo: would be more efficient to have a list sorted by name and do a binary search
	for (size_t i = 0; i < sizeof(names) / sizeof(*names); i++) {
		if (names[i] && strcmp(name, names[i]) == 0) return i;
	}
	return -1;
}

int imm_count_for_opcode(int opcode)
{
	int ret = 0;

	switch (opcode) {
	//
	// Push immediate operations
	//
	case op_push1:
	case op_push1u2:
	case op_push1u4:
	case op_push1u8:
	case op_push1i2:
	case op_push1i4:
	case op_push1i8:
	case op_push2:
	case op_push2u4:
	case op_push2u8:
	case op_push2i4:
	case op_push2i8:
	case op_push4:
	case op_push4u8:
	case op_push4i8:
	case op_push8:
		ret = 1;
		break;

	//
	// Pop operations
	//
	case op_pop1:
	case op_pop2:
	case op_pop4:
	case op_pop8:
	case op_pop:
		break;

	//
	// Duplication operations
	//
	case op_dup1:
	case op_dup2:
	case op_dup4:
	case op_dup8:
		break;

	//
	// Setting operations
	//
	case op_set1:
	case op_set2:
	case op_set4:
	case op_set8:
		break;

	//
	// Promotion Operations
	//
	case op_prom1u2:
	case op_prom1u4:
	case op_prom1u8:
	case op_prom1i2:
	case op_prom1i4:
	case op_prom1i8:
	case op_prom2u4:
	case op_prom2u8:
	case op_prom2i4:
	case op_prom2i8:
	case op_prom4u8:
	case op_prom4i8:
		break;

	//
	// Demotion Operations
	//
	case op_dem8_4:
	case op_dem8_2:
	case op_dem8_1:
	case op_dem4_2:
	case op_dem4_1:
	case op_dem2_1:
		break;

	//
	// Integer Arithmetic Operations
	//
	case op_add1:
	case op_add2:
	case op_add4:
	case op_add8:
	case op_sub1:
	case op_sub2:
	case op_sub4:
	case op_sub8:
	case op_mulu1:
	case op_mulu2:
	case op_mulu4:
	case op_mulu8:
	case op_muli1:
	case op_muli2:
	case op_muli4:
	case op_muli8:
	case op_divu1:
	case op_divu2:
	case op_divu4:
	case op_divu8:
	case op_divi1:
	case op_divi2:
	case op_divi4:
	case op_divi8:
	case op_modu1:
	case op_modu2:
	case op_modu4:
	case op_modu8:
	case op_modi1:
	case op_modi2:
	case op_modi4:
	case op_modi8:
		break;

	//
	// Bitwise shift operations
	//
	case op_lshift1:
	case op_lshift2:
	case op_lshift4:
	case op_lshift8:
	case op_rshiftu1:
	case op_rshiftu2:
	case op_rshiftu4:
	case op_rshiftu8:
	case op_rshifti1:
	case op_rshifti2:
	case op_rshifti4:
	case op_rshifti8:
		break;

	//
	// Bitwise logical operations
	//
	case op_band1:
	case op_band2:
	case op_band4:
	case op_band8:
	case op_bor1:
	case op_bor2:
	case op_bor4:
	case op_bor8:
	case op_bxor1:
	case op_bxor2:
	case op_bxor4:
	case op_bxor8:
	case op_binv1:
	case op_binv2:
	case op_binv4:
	case op_binv8:
		break;

	//
	// Boolean logical operations
	//
	case op_land1:
	case op_land2:
	case op_land4:
	case op_land8:
	case op_lor1:
	case op_lor2:
	case op_lor4:
	case op_lor8:
	case op_linv1:
	case op_linv2:
	case op_linv4:
	case op_linv8:
		break;

	//
	// Comparison operations
	//
	case op_ceq1:
	case op_ceq2:
	case op_ceq4:
	case op_ceq8:
	case op_cne1:
	case op_cne2:
	case op_cne4:
	case op_cne8:
	case op_cgtu1:
	case op_cgtu2:
	case op_cgtu4:
	case op_cgtu8:
	case op_cgti1:
	case op_cgti2:
	case op_cgti4:
	case op_cgti8:
	case op_cltu1:
	case op_cltu2:
	case op_cltu4:
	case op_cltu8:
	case op_clti1:
	case op_clti2:
	case op_clti4:
	case op_clti8:
	case op_cgeu1:
	case op_cgeu2:
	case op_cgeu4:
	case op_cgeu8:
	case op_cgei1:
	case op_cgei2:
	case op_cgei4:
	case op_cgei8:
	case op_cleu1:
	case op_cleu2:
	case op_cleu4:
	case op_cleu8:
	case op_clei1:
	case op_clei2:
	case op_clei4:
	case op_clei8:
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
	// Branching operations
	//
	case op_jmp:
		ret = 1;
		break;
	case op_jmps:
		break;
	case op_rjmpi1:
	case op_rjmpi2:
	case op_rjmpi4:
	case op_rjmpi8:
		ret = 1;
		break;

	//
	// Conditional branching operations
	//
	case op_brz1:
	case op_brz2:
	case op_brz4:
	case op_brz8:
	case op_rbrz1i1:
	case op_rbrz2i1:
	case op_rbrz4i1:
	case op_rbrz8i1:
	case op_rbrz1i2:
	case op_rbrz2i2:
	case op_rbrz4i2:
	case op_rbrz8i2:
	case op_rbrz1i4:
	case op_rbrz2i4:
	case op_rbrz4i4:
	case op_rbrz8i4:
	case op_rbrz1i8:
	case op_rbrz2i8:
	case op_rbrz4i8:
	case op_rbrz8i8:
		ret = 1;
		break;

	//
	// Memory operations
	//
	case op_load1:
	case op_load2:
	case op_load4:
	case op_load8:
	case op_loadpop1:
	case op_loadpop2:
	case op_loadpop4:
	case op_loadpop8:
	case op_loadsfp1:
	case op_loadsfp2:
	case op_loadsfp4:
	case op_loadsfp8:
	case op_loadpopsfp1:
	case op_loadpopsfp2:
	case op_loadpopsfp4:
	case op_loadpopsfp8:
	case op_store1:
	case op_store2:
	case op_store4:
	case op_store8:
	case op_storepop1:
	case op_storepop2:
	case op_storepop4:
	case op_storepop8:
	case op_storesfp1:
	case op_storesfp2:
	case op_storesfp4:
	case op_storesfp8:
	case op_storepopsfp1:
	case op_storepopsfp2:
	case op_storepopsfp4:
	case op_storepopsfp8:
		break;

	//
	// Special Operations
	//
	case op_admin:
	case op_ext:
	case op_nop:
		break;

	default:
		ret = -1;
		break;
	}

	return ret;
}

int imm_types_for_opcode(int opcode, int *dts)
{
	int ret = 0;

	switch (opcode) {
	//
	// Push immediate operations
	//
	case op_push1: *dts = dt_a1; break;
	case op_push1u2: *dts = dt_u1; break;
	case op_push1u4: *dts = dt_u1; break;
	case op_push1u8: *dts = dt_u1; break;
	case op_push1i2: *dts = dt_i1; break;
	case op_push1i4: *dts = dt_i1; break;
	case op_push1i8: *dts = dt_i1; break;
	case op_push2: *dts = dt_a2; break;
	case op_push2u4: *dts = dt_u2; break;
	case op_push2u8: *dts = dt_u2; break;
	case op_push2i4: *dts = dt_i2; break;
	case op_push2i8: *dts = dt_i2; break;
	case op_push4: *dts = dt_a4; break;
	case op_push4u8: *dts = dt_u4; break;
	case op_push4i8: *dts = dt_i4; break;
	case op_push8: *dts = dt_a8; break;

	//
	// Pop operations
	//
	case op_pop1:
	case op_pop2:
	case op_pop4:
	case op_pop8:
	case op_pop:
		break;

	//
	// Duplication operations
	//
	case op_dup1:
	case op_dup2:
	case op_dup4:
	case op_dup8:
		break;

	//
	// Setting operations
	//
	case op_set1:
	case op_set2:
	case op_set4:
	case op_set8:
		break;

	//
	// Promotion Operations
	//
	case op_prom1u2:
	case op_prom1u4:
	case op_prom1u8:
	case op_prom1i2:
	case op_prom1i4:
	case op_prom1i8:
	case op_prom2u4:
	case op_prom2u8:
	case op_prom2i4:
	case op_prom2i8:
	case op_prom4u8:
	case op_prom4i8:
		break;

	//
	// Demotion Operations
	//
	case op_dem8_4:
	case op_dem8_2:
	case op_dem8_1:
	case op_dem4_2:
	case op_dem4_1:
	case op_dem2_1:
		break;

	//
	// Integer Arithmetic Operations
	//
	case op_add1:
	case op_add2:
	case op_add4:
	case op_add8:
	case op_sub1:
	case op_sub2:
	case op_sub4:
	case op_sub8:
	case op_mulu1:
	case op_mulu2:
	case op_mulu4:
	case op_mulu8:
	case op_muli1:
	case op_muli2:
	case op_muli4:
	case op_muli8:
	case op_divu1:
	case op_divu2:
	case op_divu4:
	case op_divu8:
	case op_divi1:
	case op_divi2:
	case op_divi4:
	case op_divi8:
	case op_modu1:
	case op_modu2:
	case op_modu4:
	case op_modu8:
	case op_modi1:
	case op_modi2:
	case op_modi4:
	case op_modi8:
		break;

	//
	// Bitwise shift operations
	//
	case op_lshift1:
	case op_lshift2:
	case op_lshift4:
	case op_lshift8:
	case op_rshiftu1:
	case op_rshiftu2:
	case op_rshiftu4:
	case op_rshiftu8:
	case op_rshifti1:
	case op_rshifti2:
	case op_rshifti4:
	case op_rshifti8:
		break;

	//
	// Bitwise logical operations
	//
	case op_band1:
	case op_band2:
	case op_band4:
	case op_band8:
	case op_bor1:
	case op_bor2:
	case op_bor4:
	case op_bor8:
	case op_bxor1:
	case op_bxor2:
	case op_bxor4:
	case op_bxor8:
	case op_binv1:
	case op_binv2:
	case op_binv4:
	case op_binv8:
		break;

	//
	// Boolean logical operations
	//
	case op_land1:
	case op_land2:
	case op_land4:
	case op_land8:
	case op_lor1:
	case op_lor2:
	case op_lor4:
	case op_lor8:
	case op_linv1:
	case op_linv2:
	case op_linv4:
	case op_linv8:
		break;

	//
	// Comparison operations
	//
	case op_ceq1:
	case op_ceq2:
	case op_ceq4:
	case op_ceq8:
	case op_cne1:
	case op_cne2:
	case op_cne4:
	case op_cne8:
	case op_cgtu1:
	case op_cgtu2:
	case op_cgtu4:
	case op_cgtu8:
	case op_cgti1:
	case op_cgti2:
	case op_cgti4:
	case op_cgti8:
	case op_cltu1:
	case op_cltu2:
	case op_cltu4:
	case op_cltu8:
	case op_clti1:
	case op_clti2:
	case op_clti4:
	case op_clti8:
	case op_cgeu1:
	case op_cgeu2:
	case op_cgeu4:
	case op_cgeu8:
	case op_cgei1:
	case op_cgei2:
	case op_cgei4:
	case op_cgei8:
	case op_cleu1:
	case op_cleu2:
	case op_cleu4:
	case op_cleu8:
	case op_clei1:
	case op_clei2:
	case op_clei4:
	case op_clei8:
		break;

	//
	// Function operations
	//
	case op_call:
		*dts = dt_u8;
		break;
	case op_calls:
	case op_ret:
		break;

	//
	// Branching operations
	//
	case op_jmp: *dts = dt_u8; break;
	case op_jmps: break;
	case op_rjmpi1: *dts = dt_i1; break;
	case op_rjmpi2: *dts = dt_i2; break;
	case op_rjmpi4: *dts = dt_i4; break;
	case op_rjmpi8: *dts = dt_i8; break;

	//
	// Conditional branching operations
	//
	case op_brz1:
	case op_brz2:
	case op_brz4:
	case op_brz8:
		*dts = dt_u8;
		break;
	case op_rbrz1i1:
	case op_rbrz2i1:
	case op_rbrz4i1:
	case op_rbrz8i1:
		*dts = dt_i1;
		break;
	case op_rbrz1i2:
	case op_rbrz2i2:
	case op_rbrz4i2:
	case op_rbrz8i2:
		*dts = dt_i2;
		break;
	case op_rbrz1i4:
	case op_rbrz2i4:
	case op_rbrz4i4:
	case op_rbrz8i4:
		*dts = dt_i4;
		break;
	case op_rbrz1i8:
	case op_rbrz2i8:
	case op_rbrz4i8:
	case op_rbrz8i8:
		*dts = dt_i8;
		break;

	//
	// Memory operations
	//
	case op_load1:
	case op_load2:
	case op_load4:
	case op_load8:
	case op_loadpop1:
	case op_loadpop2:
	case op_loadpop4:
	case op_loadpop8:
	case op_loadsfp1:
	case op_loadsfp2:
	case op_loadsfp4:
	case op_loadsfp8:
	case op_loadpopsfp1:
	case op_loadpopsfp2:
	case op_loadpopsfp4:
	case op_loadpopsfp8:
	case op_store1:
	case op_store2:
	case op_store4:
	case op_store8:
	case op_storepop1:
	case op_storepop2:
	case op_storepop4:
	case op_storepop8:
	case op_storesfp1:
	case op_storesfp2:
	case op_storesfp4:
	case op_storesfp8:
	case op_storepopsfp1:
	case op_storepopsfp2:
	case op_storepopsfp4:
	case op_storepopsfp8:
		break;

	//
	// Special Operations
	//
	case op_admin:
		// @todo
	case op_ext:
	case op_nop:
		break;

	default:
		ret = -1;
		break;
	}

	return ret;
}
