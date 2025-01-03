// starch.h
//
// Starch is a 64-bit stack-based virtual machine architecture.

#pragma once

#include <stdint.h>

enum {

	op_invalid = 0x00, // Invalid operation

	//
	// Push immediate operations
	//
	op_push1   = 0x01, // Push 1 byte imm
	op_push1u2 = 0x02, // Push 1 byte unsigned imm as 2 byte
	op_push1u4 = 0x03, // Push 1 byte unsigned imm as 4 byte
	op_push1u8 = 0x04, // Push 1 byte unsigned imm as 8 byte
	op_push1i2 = 0x05, // Push 1 byte signed imm as 2 byte
	op_push1i4 = 0x06, // Push 1 byte signed imm as 4 byte
	op_push1i8 = 0x07, // Push 1 byte signed imm as 8 byte
	op_push2   = 0x08, // Push 2 byte imm
	op_push2u4 = 0x09, // Push 2 byte unsigned imm as 4 byte
	op_push2u8 = 0x0a, // Push 2 byte unsigned imm as 8 byte
	op_push2i4 = 0x0b, // Push 2 byte signed imm as 4 byte
	op_push2i8 = 0x0c, // Push 2 byte signed imm as 8 byte
	op_push4   = 0x0d, // Push 4 byte imm
	op_push4u8 = 0x0e, // Push 4 byte unsigned imm as 8 byte
	op_push4i8 = 0x0f, // Push 4 byte unsigned imm as 8 byte
	op_push8   = 0x10, // Push 8 byte imm

	//
	// Pop operations
	//
	op_pop1, // Pop top 1 byte
	op_pop2, // Pop top 2 byte
	op_pop4, // Pop top 4 byte
	op_pop8, // Pop top 8 byte
	op_pop, // Pop by addr offset on stack

	//
	// Duplication operations
	//
	op_dup1, // Duplicate top 1 byte
	op_dup2, // Duplicate top 2 byte
	op_dup4, // Duplicate top 4 byte
	op_dup8, // Duplicate top 8 byte

	//
	// Setting operations
	//
	op_set1, // Pop 1 byte and set top 1 byte
	op_set2, // Pop 2 byte and set top 2 byte
	op_set4, // Pop 4 byte and set top 4 byte
	op_set8, // Pop 8 byte and set top 8 byte

	//
	// Promotion Operations
	//
	op_prom1u2, // Promote top 1 byte to 2 byte unsigned
	op_prom1u4, // Promote top 1 byte to 4 byte unsigned
	op_prom1u8, // Promote top 1 byte to 8 byte unsigned
	op_prom1i2, // Promote top 1 byte to 2 byte signed
	op_prom1i4, // Promote top 1 byte to 4 byte signed
	op_prom1i8, // Promote top 1 byte to 8 byte signed
	op_prom2u4, // Promote top 2 byte to 4 byte unsigned
	op_prom2u8, // Promote top 2 byte to 8 byte unsigned
	op_prom2i4, // Promote top 2 byte to 4 byte signed
	op_prom2i8, // Promote top 2 byte to 8 byte signed
	op_prom4u8, // Promote top 4 byte to 8 byte unsigned
	op_prom4i8, // Promote top 4 byte to 8 byte signed

	//
	// Demotion Operations
	//
	op_dem8_4, // Demote top 8 byte to 4 byte
	op_dem8_2, // Demote top 8 byte to 2 byte
	op_dem8_1, // Demote top 8 byte to 1 byte
	op_dem4_2, // Demote top 4 byte to 2 byte
	op_dem4_1, // Demote top 4 byte to 1 byte
	op_dem2_1, // Demote top 2 byte to 1 byte

	//
	// Integer Arithmetic Operations
	//
	op_add1, // Add 1 byte integers replacing with sum
	op_add2, // Add 2 byte integers replacing with sum
	op_add4, // Add 4 byte integers replacing with sum
	op_add8, // Add 8 byte integers replacing with sum
	op_sub1, // Subtract 1 byte integers replacing with difference
	op_sub2, // Subtract 2 byte integers replacing with difference
	op_sub4, // Subtract 4 byte integers replacing with difference
	op_sub8, // Subtract 8 byte integers replacing with difference
	op_mulu1, // Multiply 1 byte unsigned integers replacing with product
	op_mulu2, // Multiply 2 byte unsigned integers replacing with product
	op_mulu4, // Multiply 4 byte unsigned integers replacing with product
	op_mulu8, // Multiply 8 byte unsigned integers replacing with product
	op_muli1, // Multiply 1 byte signed integers replacing with product
	op_muli2, // Multiply 2 byte signed integers replacing with product
	op_muli4, // Multiply 4 byte signed integers replacing with product
	op_muli8, // Multiply 8 byte signed integers replacing with product
	op_divu1, // Divide 1 byte unsigned integers replacing with quotient
	op_divu2, // Divide 2 byte unsigned integers replacing with quotient
	op_divu4, // Divide 4 byte unsigned integers replacing with quotient
	op_divu8, // Divide 8 byte unsigned integers replacing with quotient
	op_divi1, // Divide 1 byte signed integers replacing with quotient
	op_divi2, // Divide 2 byte signed integers replacing with quotient
	op_divi4, // Divide 4 byte signed integers replacing with quotient
	op_divi8, // Divide 8 byte signed integers replacing with quotient
	op_modu1, // Divide 1 byte unsigned integers replacing with remainder
	op_modu2, // Divide 2 byte unsigned integers replacing with remainder
	op_modu4, // Divide 4 byte unsigned integers replacing with remainder
	op_modu8, // Divide 8 byte unsigned integers replacing with remainder
	op_modi1, // Divide 1 byte signed integers replacing with remainder
	op_modi2, // Divide 2 byte signed integers replacing with remainder
	op_modi4, // Divide 4 byte signed integers replacing with remainder
	op_modi8, // Divide 8 byte signed integers replacing with remainder

	//
	// Bitwise shift operations
	//
	op_lshift1, // Shift 1 byte left by top 1 byte popped
	op_lshift2, // Shift 2 byte left by top 1 byte popped
	op_lshift4, // Shift 4 byte left by top 1 byte popped
	op_lshift8, // Shift 8 byte left by top 1 byte popped
	op_rshiftu1, // Shift 1 byte unsigned left by top 1 byte popped
	op_rshiftu2, // Shift 2 byte unsigned left by top 1 byte popped
	op_rshiftu4, // Shift 4 byte unsigned left by top 1 byte popped
	op_rshiftu8, // Shift 8 byte unsigned left by top 1 byte popped
	op_rshifti1, // Shift 1 byte signed left by top 1 byte popped
	op_rshifti2, // Shift 2 byte signed left by top 1 byte popped
	op_rshifti4, // Shift 4 byte signed left by top 1 byte popped
	op_rshifti8, // Shift 8 byte signed left by top 1 byte popped

	//
	// Bitwise logical operations
	//
	op_band1, // Bitwise and 1 byte integers replacing with result
	op_band2, // Bitwise and 2 byte integers replacing with result
	op_band4, // Bitwise and 4 byte integers replacing with result
	op_band8, // Bitwise and 8 byte integers replacing with result
	op_bor1, // Bitwise or 1 byte integers replacing with result
	op_bor2, // Bitwise or 2 byte integers replacing with result
	op_bor4, // Bitwise or 4 byte integers replacing with result
	op_bor8, // Bitwise or 8 byte integers replacing with result
	op_bxor1, // Bitwise xor 1 byte integers replacing with result
	op_bxor2, // Bitwise xor 2 byte integers replacing with result
	op_bxor4, // Bitwise xor 4 byte integers replacing with result
	op_bxor8, // Bitwise xor 8 byte integers replacing with result
	op_binv1, // Bitwise invert top 1 byte
	op_binv2, // Bitwise invert top 2 byte
	op_binv4, // Bitwise invert top 4 byte
	op_binv8, // Bitwise invert top 8 byte

	//
	// Boolean logical operations
	//
	op_land1, // Boolean and 1 byte integers replacing with result
	op_land2, // Boolean and 2 byte integers replacing with result
	op_land4, // Boolean and 4 byte integers replacing with result
	op_land8, // Boolean and 8 byte integers replacing with result
	op_lor1, // Boolean or 1 byte integers replacing with result
	op_lor2, // Boolean or 2 byte integers replacing with result
	op_lor4, // Boolean or 4 byte integers replacing with result
	op_lor8, // Boolean or 8 byte integers replacing with result
	op_linv1, // Boolean invert top 1 byte
	op_linv2, // Boolean invert top 2 byte
	op_linv4, // Boolean invert top 4 byte
	op_linv8, // Boolean invert top 8 byte

	//
	// Comparison operations
	//
	op_ceq1, // Push whether equal top 1 byte popped integers
	op_ceq2, // Push whether equal top 2 byte popped integers
	op_ceq4, // Push whether equal top 4 byte popped integers
	op_ceq8, // Push whether equal top 8 byte popped integers
	op_cne1, // Push whether not equal top 1 byte popped integers
	op_cne2, // Push whether not equal top 2 byte popped integers
	op_cne4, // Push whether not equal top 4 byte popped integers
	op_cne8, // Push whether not equal top 8 byte popped integers
	op_cgtu1, // Push whether top greater of top 1 byte popped unsigned integers
	op_cgtu2, // Push whether top greater of top 2 byte popped unsigned integers
	op_cgtu4, // Push whether top greater of top 4 byte popped unsigned integers
	op_cgtu8, // Push whether top greater of top 8 byte popped unsigned integers
	op_cgti1, // Push whether top greater of top 1 byte popped signed integers
	op_cgti2, // Push whether top greater of top 2 byte popped signed integers
	op_cgti4, // Push whether top greater of top 4 byte popped signed integers
	op_cgti8, // Push whether top greater of top 8 byte popped signed integers
	op_cltu1, // Push whether top less of top 1 byte popped unsigned integers
	op_cltu2, // Push whether top less of top 2 byte popped unsigned integers
	op_cltu4, // Push whether top less of top 4 byte popped unsigned integers
	op_cltu8, // Push whether top less of top 8 byte popped unsigned integers
	op_clti1, // Push whether top less of top 1 byte popped signed integers
	op_clti2, // Push whether top less of top 2 byte popped signed integers
	op_clti4, // Push whether top less of top 4 byte popped signed integers
	op_clti8, // Push whether top less of top 8 byte popped signed integers
	op_cgeu1, // Push whether top greater or equal of top 1 byte popped unsigned integers
	op_cgeu2, // Push whether top greater or equal of top 2 byte popped unsigned integers
	op_cgeu4, // Push whether top greater or equal of top 4 byte popped unsigned integers
	op_cgeu8, // Push whether top greater or equal of top 8 byte popped unsigned integers
	op_cgei1, // Push whether top greater or equal of top 1 byte popped signed integers
	op_cgei2, // Push whether top greater or equal of top 2 byte popped signed integers
	op_cgei4, // Push whether top greater or equal of top 4 byte popped signed integers
	op_cgei8, // Push whether top greater or equal of top 8 byte popped signed integers
	op_cleu1, // Push whether top less or equal of top 1 byte popped unsigned integers
	op_cleu2, // Push whether top less or equal of top 2 byte popped unsigned integers
	op_cleu4, // Push whether top less or equal of top 4 byte popped unsigned integers
	op_cleu8, // Push whether top less or equal of top 8 byte popped unsigned integers
	op_clei1, // Push whether top less or equal of top 1 byte popped signed integers
	op_clei2, // Push whether top less or equal of top 2 byte popped signed integers
	op_clei4, // Push whether top less or equal of top 4 byte popped signed integers
	op_clei8, // Push whether top less or equal of top 8 byte popped signed integers

	//
	// Function operations
	//
	op_call, // Call function at 8 byte imm addr
	op_calls, // Call function at top 8 byte popped addr
	op_ret, // Return from current function

	//
	// Branching operations
	//
	op_jmp, // Jump to 8 byte imm addr
	op_jmps, // Jump to top 8 byte popped addr
	op_rjmpi1, // Relative jump by signed 1 byte imm integer
	op_rjmpi2, // Relative jump by signed 2 byte imm integer
	op_rjmpi4, // Relative jump by signed 4 byte imm integer
	op_rjmpi8, // Relative jump by signed 8 byte imm integer

	//
	// Conditional branching operations
	//
	op_brz1, // Jump to 8 byte imm addr if top 1 byte popped non-zero
	op_brz2, // Jump to 8 byte imm addr if top 2 byte popped non-zero
	op_brz4, // Jump to 8 byte imm addr if top 4 byte popped non-zero
	op_brz8, // Jump to 8 byte imm addr if top 8 byte popped non-zero
	op_rbrz1i1, // Relative jump by signed 1 byte imm addr if top 1 byte popped non-zero
	op_rbrz2i1, // Relative jump by signed 1 byte imm addr if top 2 byte popped non-zero
	op_rbrz4i1, // Relative jump by signed 1 byte imm addr if top 4 byte popped non-zero
	op_rbrz8i1, // Relative jump by signed 1 byte imm addr if top 8 byte popped non-zero
	op_rbrz1i2, // Relative jump by signed 2 byte imm addr if top 1 byte popped non-zero
	op_rbrz2i2, // Relative jump by signed 2 byte imm addr if top 2 byte popped non-zero
	op_rbrz4i2, // Relative jump by signed 2 byte imm addr if top 4 byte popped non-zero
	op_rbrz8i2, // Relative jump by signed 2 byte imm addr if top 8 byte popped non-zero
	op_rbrz1i4, // Relative jump by signed 4 byte imm addr if top 1 byte popped non-zero
	op_rbrz2i4, // Relative jump by signed 4 byte imm addr if top 2 byte popped non-zero
	op_rbrz4i4, // Relative jump by signed 4 byte imm addr if top 4 byte popped non-zero
	op_rbrz8i4, // Relative jump by signed 4 byte imm addr if top 8 byte popped non-zero
	op_rbrz1i8, // Relative jump by signed 8 byte imm addr if top 1 byte popped non-zero
	op_rbrz2i8, // Relative jump by signed 8 byte imm addr if top 2 byte popped non-zero
	op_rbrz4i8, // Relative jump by signed 8 byte imm addr if top 4 byte popped non-zero
	op_rbrz8i8, // Relative jump by signed 8 byte imm addr if top 8 byte popped non-zero

	//
	// Memory operations
	//
	op_load1, // Push 1 byte from top 8 byte address
	op_load2, // Push 2 byte from top 8 byte address
	op_load4, // Push 4 byte from top 8 byte address
	op_load8, // Push 8 byte from top 8 byte address
	op_loadpop1, // Push 1 byte from top 8 byte popped address
	op_loadpop2, // Push 2 byte from top 8 byte popped address
	op_loadpop4, // Push 4 byte from top 8 byte popped address
	op_loadpop8, // Push 8 byte from top 8 byte popped address
	op_loadsfp1, // Push 1 byte from top 8 byte signed offset from SFP
	op_loadsfp2, // Push 2 byte from top 8 byte signed offset from SFP
	op_loadsfp4, // Push 4 byte from top 8 byte signed offset from SFP
	op_loadsfp8, // Push 8 byte from top 8 byte signed offset from SFP
	op_loadpopsfp1, // Push 1 byte from top 8 byte popped signed offset from SFP
	op_loadpopsfp2, // Push 2 byte from top 8 byte popped signed offset from SFP
	op_loadpopsfp4, // Push 4 byte from top 8 byte popped signed offset from SFP
	op_loadpopsfp8, // Push 8 byte from top 8 byte popped signed offset from SFP
	op_store1, // Store 1 byte to top 8 byte address
	op_store2, // Store 2 byte to top 8 byte address
	op_store4, // Store 4 byte to top 8 byte address
	op_store8, // Store 8 byte to top 8 byte address
	op_storepop1, // Store 1 byte to top 8 byte popped address
	op_storepop2, // Store 2 byte to top 8 byte popped address
	op_storepop4, // Store 4 byte to top 8 byte popped address
	op_storepop8, // Store 8 byte to top 8 byte popped address
	op_storesfp1, // Store 1 byte to top 8 byte signed offset from SFP
	op_storesfp2, // Store 2 byte to top 8 byte signed offset from SFP
	op_storesfp4, // Store 4 byte to top 8 byte signed offset from SFP
	op_storesfp8, // Store 8 byte to top 8 byte signed offset from SFP
	op_storepopsfp1, // Store 1 byte to top 8 byte popped signed offset from SFP
	op_storepopsfp2, // Store 2 byte to top 8 byte popped signed offset from SFP
	op_storepopsfp4, // Store 4 byte to top 8 byte popped signed offset from SFP
	op_storepopsfp8, // Store 8 byte to top 8 byte popped signed offset from SFP

	//
	// Special Operations
	//
	op_admin = 0xfd, // Administrative operations
	op_ext = 0xfe, // Introduces an extended operation
	op_nop = 0xff // No op
};

// Returns the name of the given opcode, or NULL on error
const char *name_for_opcode(int);

// Returns the opcode for the given name, or -1 on error
int opcode_for_name(const char*);

// Returns the number of immediate arguments required by the given opcode, or -1 on error
int imm_count_for_opcode(int);

//
// Data types
//
enum {
	dt_a1, // Any 1 byte quantity
	dt_a2, // Any 2 byte quantity
	dt_a4, // Any 4 byte quantity
	dt_a8, // Any 8 byte quantity
	dt_i1, // A signed 1 byte integer
	dt_i2, // A signed 2 byte integer
	dt_i4, // A signed 4 byte integer
	dt_i8, // A signed 8 byte integer
	dt_u1, // An unsigned 1 byte integer
	dt_u2, // An unsigned 2 byte integer
	dt_u4, // An unsigned 4 byte integer
	dt_u8, // An unsigned 8 byte integer
};

// Get the data types of the immediate arguments for the given opcode.
// Use imm_count_for_opcode() to determine the minimum length of dts.
// Returns -1 on error.
int imm_types_for_opcode(int opcode, int *dts);
