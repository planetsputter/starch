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
	op_push8     = 0x01, // Push 1 byte imm
	op_push8u16  = 0x02, // Push 1 byte unsigned imm as 2 byte
	op_push8u32  = 0x03, // Push 1 byte unsigned imm as 4 byte
	op_push8u64  = 0x04, // Push 1 byte unsigned imm as 8 byte
	op_push8i16  = 0x05, // Push 1 byte signed imm as 2 byte
	op_push8i32  = 0x06, // Push 1 byte signed imm as 4 byte
	op_push8i64  = 0x07, // Push 1 byte signed imm as 8 byte
	op_push16    = 0x08, // Push 2 byte imm
	op_push16u32 = 0x09, // Push 2 byte unsigned imm as 4 byte
	op_push16u64 = 0x0a, // Push 2 byte unsigned imm as 8 byte
	op_push16i32 = 0x0b, // Push 2 byte signed imm as 4 byte
	op_push16i64 = 0x0c, // Push 2 byte signed imm as 8 byte
	op_push32    = 0x0d, // Push 4 byte imm
	op_push32u64 = 0x0e, // Push 4 byte unsigned imm as 8 byte
	op_push32i64 = 0x0f, // Push 4 byte unsigned imm as 8 byte
	op_push64    = 0x10, // Push 8 byte imm

	//
	// Pop operations
	//
	op_pop8  = 0x11, // Pop top 1 byte
	op_pop16 = 0x12, // Pop top 2 byte
	op_pop32 = 0x13, // Pop top 4 byte
	op_pop64 = 0x14, // Pop top 8 byte

	//
	// Duplication operations
	//
	op_dup8  = 0x15, // Duplicate top 1 byte
	op_dup16 = 0x16, // Duplicate top 2 byte
	op_dup32 = 0x17, // Duplicate top 4 byte
	op_dup64 = 0x18, // Duplicate top 8 byte

	//
	// Setting operations
	//
	op_set8  = 0x19, // Pop 1 byte and set top 1 byte
	op_set16 = 0x1a, // Pop 2 byte and set top 2 byte
	op_set32 = 0x1b, // Pop 4 byte and set top 4 byte
	op_set64 = 0x1c, // Pop 8 byte and set top 8 byte

	//
	// Promotion operations
	//
	op_prom8u16  = 0x1d, // Promote top 1 byte to 2 byte unsigned
	op_prom8u32  = 0x1e, // Promote top 1 byte to 4 byte unsigned
	op_prom8u64  = 0x1f, // Promote top 1 byte to 8 byte unsigned
	op_prom8i16  = 0x20, // Promote top 1 byte to 2 byte signed
	op_prom8i32  = 0x21, // Promote top 1 byte to 4 byte signed
	op_prom8i64  = 0x22, // Promote top 1 byte to 8 byte signed
	op_prom16u32 = 0x23, // Promote top 2 byte to 4 byte unsigned
	op_prom16u64 = 0x24, // Promote top 2 byte to 8 byte unsigned
	op_prom16i32 = 0x25, // Promote top 2 byte to 4 byte signed
	op_prom16i64 = 0x26, // Promote top 2 byte to 8 byte signed
	op_prom32u64 = 0x27, // Promote top 4 byte to 8 byte unsigned
	op_prom32i64 = 0x28, // Promote top 4 byte to 8 byte signed

	//
	// Demotion operations
	//
	op_dem64to32 = 0x29, // Demote top 8 byte to 4 byte
	op_dem64to16 = 0x2a, // Demote top 8 byte to 2 byte
	op_dem64to8  = 0x2b, // Demote top 8 byte to 1 byte
	op_dem32to16 = 0x2c, // Demote top 4 byte to 2 byte
	op_dem32to8  = 0x2d, // Demote top 4 byte to 1 byte
	op_dem16to8  = 0x2e, // Demote top 2 byte to 1 byte

	//
	// Integer arithmetic operations
	//
	op_add8   = 0x2f, // Add 1 byte integers replacing with sum
	op_add16  = 0x30, // Add 2 byte integers replacing with sum
	op_add32  = 0x31, // Add 4 byte integers replacing with sum
	op_add64  = 0x32, // Add 8 byte integers replacing with sum
	op_sub8   = 0x33, // Subtract 1 byte integers replacing with difference
	op_sub16  = 0x34, // Subtract 2 byte integers replacing with difference
	op_sub32  = 0x35, // Subtract 4 byte integers replacing with difference
	op_sub64  = 0x36, // Subtract 8 byte integers replacing with difference
	op_mulu8  = 0x37, // Multiply 1 byte unsigned integers replacing with product
	op_mulu16 = 0x38, // Multiply 2 byte unsigned integers replacing with product
	op_mulu32 = 0x39, // Multiply 4 byte unsigned integers replacing with product
	op_mulu64 = 0x3a, // Multiply 8 byte unsigned integers replacing with product
	op_muli8  = 0x3b, // Multiply 1 byte signed integers replacing with product
	op_muli16 = 0x3c, // Multiply 2 byte signed integers replacing with product
	op_muli32 = 0x3d, // Multiply 4 byte signed integers replacing with product
	op_muli64 = 0x3e, // Multiply 8 byte signed integers replacing with product
	op_divu8  = 0x3f, // Divide 1 byte unsigned integers replacing with quotient
	op_divu16 = 0x40, // Divide 2 byte unsigned integers replacing with quotient
	op_divu32 = 0x41, // Divide 4 byte unsigned integers replacing with quotient
	op_divu64 = 0x42, // Divide 8 byte unsigned integers replacing with quotient
	op_divi8  = 0x43, // Divide 1 byte signed integers replacing with quotient
	op_divi16 = 0x44, // Divide 2 byte signed integers replacing with quotient
	op_divi32 = 0x45, // Divide 4 byte signed integers replacing with quotient
	op_divi64 = 0x46, // Divide 8 byte signed integers replacing with quotient
	op_modu8  = 0x47, // Divide 1 byte unsigned integers replacing with remainder
	op_modu16 = 0x48, // Divide 2 byte unsigned integers replacing with remainder
	op_modu32 = 0x49, // Divide 4 byte unsigned integers replacing with remainder
	op_modu64 = 0x4a, // Divide 8 byte unsigned integers replacing with remainder
	op_modi8  = 0x4b, // Divide 1 byte signed integers replacing with remainder
	op_modi16 = 0x4c, // Divide 2 byte signed integers replacing with remainder
	op_modi32 = 0x4d, // Divide 4 byte signed integers replacing with remainder
	op_modi64 = 0x4e, // Divide 8 byte signed integers replacing with remainder

	//
	// Bitwise shift operations
	//
	op_lshift8   = 0x4f, // Shift 1 byte left by top 1 byte popped
	op_lshift16  = 0x50, // Shift 2 byte left by top 1 byte popped
	op_lshift32  = 0x51, // Shift 4 byte left by top 1 byte popped
	op_lshift64  = 0x52, // Shift 8 byte left by top 1 byte popped
	op_rshiftu8  = 0x53, // Shift 1 byte unsigned left by top 1 byte popped
	op_rshiftu16 = 0x54, // Shift 2 byte unsigned left by top 1 byte popped
	op_rshiftu32 = 0x55, // Shift 4 byte unsigned left by top 1 byte popped
	op_rshiftu64 = 0x56, // Shift 8 byte unsigned left by top 1 byte popped
	op_rshifti8  = 0x57, // Shift 1 byte signed left by top 1 byte popped
	op_rshifti16 = 0x58, // Shift 2 byte signed left by top 1 byte popped
	op_rshifti32 = 0x59, // Shift 4 byte signed left by top 1 byte popped
	op_rshifti64 = 0x5a, // Shift 8 byte signed left by top 1 byte popped

	//
	// Bitwise logical operations
	//
	op_band8  = 0x5b, // Bitwise and 1 byte integers replacing with result
	op_band16 = 0x5c, // Bitwise and 2 byte integers replacing with result
	op_band32 = 0x5d, // Bitwise and 4 byte integers replacing with result
	op_band64 = 0x5e, // Bitwise and 8 byte integers replacing with result
	op_bor8   = 0x5f, // Bitwise or 1 byte integers replacing with result
	op_bor16  = 0x60, // Bitwise or 2 byte integers replacing with result
	op_bor32  = 0x61, // Bitwise or 4 byte integers replacing with result
	op_bor64  = 0x62, // Bitwise or 8 byte integers replacing with result
	op_bxor8  = 0x63, // Bitwise xor 1 byte integers replacing with result
	op_bxor16 = 0x64, // Bitwise xor 2 byte integers replacing with result
	op_bxor32 = 0x65, // Bitwise xor 4 byte integers replacing with result
	op_bxor64 = 0x66, // Bitwise xor 8 byte integers replacing with result
	op_binv8  = 0x67, // Bitwise invert top 1 byte
	op_binv16 = 0x68, // Bitwise invert top 2 byte
	op_binv32 = 0x69, // Bitwise invert top 4 byte
	op_binv64 = 0x6a, // Bitwise invert top 8 byte

	//
	// Boolean logical operations
	//
	op_land8  = 0x6b, // Boolean and 1 byte integers replacing with result
	op_land16 = 0x6c, // Boolean and 2 byte integers replacing with result
	op_land32 = 0x6d, // Boolean and 4 byte integers replacing with result
	op_land64 = 0x6e, // Boolean and 8 byte integers replacing with result
	op_lor8   = 0x6f, // Boolean or 1 byte integers replacing with result
	op_lor16  = 0x70, // Boolean or 2 byte integers replacing with result
	op_lor32  = 0x71, // Boolean or 4 byte integers replacing with result
	op_lor64  = 0x72, // Boolean or 8 byte integers replacing with result
	op_linv8  = 0x73, // Boolean invert top 1 byte
	op_linv16 = 0x74, // Boolean invert top 2 byte
	op_linv32 = 0x75, // Boolean invert top 4 byte
	op_linv64 = 0x76, // Boolean invert top 8 byte

	//
	// Comparison operations
	//
	op_ceq8   = 0x77, // Push whether equal top 1 byte popped integers
	op_ceq16  = 0x78, // Push whether equal top 2 byte popped integers
	op_ceq32  = 0x79, // Push whether equal top 4 byte popped integers
	op_ceq64  = 0x7a, // Push whether equal top 8 byte popped integers
	op_cne8   = 0x7b, // Push whether not equal top 1 byte popped integers
	op_cne16  = 0x7c, // Push whether not equal top 2 byte popped integers
	op_cne32  = 0x7d, // Push whether not equal top 4 byte popped integers
	op_cne64  = 0x7e, // Push whether not equal top 8 byte popped integers
	op_cgtu8  = 0x7f,  // Push whether top greater of top 1 byte popped unsigned integers
	op_cgtu16 = 0x80, // Push whether top greater of top 2 byte popped unsigned integers
	op_cgtu32 = 0x81, // Push whether top greater of top 4 byte popped unsigned integers
	op_cgtu64 = 0x82, // Push whether top greater of top 8 byte popped unsigned integers
	op_cgti8  = 0x83,  // Push whether top greater of top 1 byte popped signed integers
	op_cgti16 = 0x84, // Push whether top greater of top 2 byte popped signed integers
	op_cgti32 = 0x85, // Push whether top greater of top 4 byte popped signed integers
	op_cgti64 = 0x86, // Push whether top greater of top 8 byte popped signed integers
	op_cltu8  = 0x87,  // Push whether top less of top 1 byte popped unsigned integers
	op_cltu16 = 0x88, // Push whether top less of top 2 byte popped unsigned integers
	op_cltu32 = 0x89, // Push whether top less of top 4 byte popped unsigned integers
	op_cltu64 = 0x8a, // Push whether top less of top 8 byte popped unsigned integers
	op_clti8  = 0x8b,  // Push whether top less of top 1 byte popped signed integers
	op_clti16 = 0x8c, // Push whether top less of top 2 byte popped signed integers
	op_clti32 = 0x8d, // Push whether top less of top 4 byte popped signed integers
	op_clti64 = 0x8e, // Push whether top less of top 8 byte popped signed integers
	op_cgeu8  = 0x8f,  // Push whether top greater or equal of top 1 byte popped unsigned integers
	op_cgeu16 = 0x90, // Push whether top greater or equal of top 2 byte popped unsigned integers
	op_cgeu32 = 0x91, // Push whether top greater or equal of top 4 byte popped unsigned integers
	op_cgeu64 = 0x92, // Push whether top greater or equal of top 8 byte popped unsigned integers
	op_cgei8  = 0x93,  // Push whether top greater or equal of top 1 byte popped signed integers
	op_cgei16 = 0x94, // Push whether top greater or equal of top 2 byte popped signed integers
	op_cgei32 = 0x95, // Push whether top greater or equal of top 4 byte popped signed integers
	op_cgei64 = 0x96, // Push whether top greater or equal of top 8 byte popped signed integers
	op_cleu8  = 0x97,  // Push whether top less or equal of top 1 byte popped unsigned integers
	op_cleu16 = 0x98, // Push whether top less or equal of top 2 byte popped unsigned integers
	op_cleu32 = 0x99, // Push whether top less or equal of top 4 byte popped unsigned integers
	op_cleu64 = 0x9a, // Push whether top less or equal of top 8 byte popped unsigned integers
	op_clei8  = 0x9b,  // Push whether top less or equal of top 1 byte popped signed integers
	op_clei16 = 0x9c, // Push whether top less or equal of top 2 byte popped signed integers
	op_clei32 = 0x9d, // Push whether top less or equal of top 4 byte popped signed integers
	op_clei64 = 0x9e, // Push whether top less or equal of top 8 byte popped signed integers

	//
	// Function operations
	//
	op_call  = 0x9f, // Call function at 8 byte imm addr
	op_calls = 0xa0, // Call function at top 8 byte popped addr
	op_ret   = 0xa1, // Return from current function

	//
	// Branching operations
	//
	op_jmp     = 0xa2, // Jump to 8 byte imm addr
	op_jmps    = 0xa3, // Jump to top 8 byte popped addr
	op_rjmpi8  = 0xa4, // Relative jump by signed 1 byte imm integer
	op_rjmpi16 = 0xa5, // Relative jump by signed 2 byte imm integer
	op_rjmpi32 = 0xa6, // Relative jump by signed 4 byte imm integer
	op_rjmpi64 = 0xa7, // Relative jump by signed 8 byte imm integer

	//
	// Conditional branching operations
	//
	op_brz8      = 0xa8, // Jump to 8 byte imm addr if top 1 byte popped non-zero
	op_brz16     = 0xa9, // Jump to 8 byte imm addr if top 2 byte popped non-zero
	op_brz32     = 0xaa, // Jump to 8 byte imm addr if top 4 byte popped non-zero
	op_brz64     = 0xab, // Jump to 8 byte imm addr if top 8 byte popped non-zero
	op_rbrz8i8   = 0xac, // Relative jump by signed 1 byte imm addr if top 1 byte popped non-zero
	op_rbrz16i8  = 0xad, // Relative jump by signed 1 byte imm addr if top 2 byte popped non-zero
	op_rbrz32i8  = 0xae, // Relative jump by signed 1 byte imm addr if top 4 byte popped non-zero
	op_rbrz64i8  = 0xaf, // Relative jump by signed 1 byte imm addr if top 8 byte popped non-zero
	op_rbrz8i16  = 0xb0, // Relative jump by signed 2 byte imm addr if top 1 byte popped non-zero
	op_rbrz16i16 = 0xb1, // Relative jump by signed 2 byte imm addr if top 2 byte popped non-zero
	op_rbrz32i16 = 0xb2, // Relative jump by signed 2 byte imm addr if top 4 byte popped non-zero
	op_rbrz64i16 = 0xb3, // Relative jump by signed 2 byte imm addr if top 8 byte popped non-zero
	op_rbrz8i32  = 0xb4, // Relative jump by signed 4 byte imm addr if top 1 byte popped non-zero
	op_rbrz16i32 = 0xb5, // Relative jump by signed 4 byte imm addr if top 2 byte popped non-zero
	op_rbrz32i32 = 0xb6, // Relative jump by signed 4 byte imm addr if top 4 byte popped non-zero
	op_rbrz64i32 = 0xb7, // Relative jump by signed 4 byte imm addr if top 8 byte popped non-zero
	op_rbrz8i64  = 0xb8, // Relative jump by signed 8 byte imm addr if top 1 byte popped non-zero
	op_rbrz16i64 = 0xb9, // Relative jump by signed 8 byte imm addr if top 2 byte popped non-zero
	op_rbrz32i64 = 0xba, // Relative jump by signed 8 byte imm addr if top 4 byte popped non-zero
	op_rbrz64i64 = 0xbb, // Relative jump by signed 8 byte imm addr if top 8 byte popped non-zero

	//
	// Memory operations
	//
	op_load8         = 0xbc, // Push 1 byte from top 8 byte address
	op_load16        = 0xbd, // Push 2 byte from top 8 byte address
	op_load32        = 0xbe, // Push 4 byte from top 8 byte address
	op_load64        = 0xbf, // Push 8 byte from top 8 byte address
	op_loadpop8      = 0xc0, // Push 1 byte from top 8 byte popped address
	op_loadpop16     = 0xc1, // Push 2 byte from top 8 byte popped address
	op_loadpop32     = 0xc2, // Push 4 byte from top 8 byte popped address
	op_loadpop64     = 0xc3, // Push 8 byte from top 8 byte popped address
	op_loadsfp8      = 0xc4, // Push 1 byte from top 8 byte signed offset from SFP
	op_loadsfp16     = 0xc5, // Push 2 byte from top 8 byte signed offset from SFP
	op_loadsfp32     = 0xc6, // Push 4 byte from top 8 byte signed offset from SFP
	op_loadsfp64     = 0xc7, // Push 8 byte from top 8 byte signed offset from SFP
	op_loadpopsfp8   = 0xc8, // Push 1 byte from top 8 byte popped signed offset from SFP
	op_loadpopsfp16  = 0xc9, // Push 2 byte from top 8 byte popped signed offset from SFP
	op_loadpopsfp32  = 0xca, // Push 4 byte from top 8 byte popped signed offset from SFP
	op_loadpopsfp64  = 0xcb, // Push 8 byte from top 8 byte popped signed offset from SFP
	op_store8        = 0xcc, // Store 1 byte to top 8 byte address
	op_store16       = 0xcd, // Store 2 byte to top 8 byte address
	op_store32       = 0xce, // Store 4 byte to top 8 byte address
	op_store64       = 0xcf, // Store 8 byte to top 8 byte address
	op_storepop8     = 0xd0, // Store 1 byte to top 8 byte popped address
	op_storepop16    = 0xd1, // Store 2 byte to top 8 byte popped address
	op_storepop32    = 0xd2, // Store 4 byte to top 8 byte popped address
	op_storepop64    = 0xd3, // Store 8 byte to top 8 byte popped address
	op_storesfp8     = 0xd4, // Store 1 byte to top 8 byte signed offset from SFP
	op_storesfp16    = 0xd5, // Store 2 byte to top 8 byte signed offset from SFP
	op_storesfp32    = 0xd6, // Store 4 byte to top 8 byte signed offset from SFP
	op_storesfp64    = 0xd7, // Store 8 byte to top 8 byte signed offset from SFP
	op_storepopsfp8  = 0xd8, // Store 1 byte to top 8 byte popped signed offset from SFP
	op_storepopsfp16 = 0xd9, // Store 2 byte to top 8 byte popped signed offset from SFP
	op_storepopsfp32 = 0xda, // Store 4 byte to top 8 byte popped signed offset from SFP
	op_storepopsfp64 = 0xdb, // Store 8 byte to top 8 byte popped signed offset from SFP

	//
	// Special Operations
	//
	op_setsbp, // Set SBP to top 8 byte address
	op_setsfp, // Set SFP to top 8 byte address
	op_setsp,  // Set SP to top 8 byte address
	op_halt, // Halts the processor
	op_ext   = 0xfe, // Introduces an extended operation
	op_nop   = 0xff, // No op
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
	dt_a8, // Any 1 byte quantity
	dt_a16, // Any 2 byte quantity
	dt_a32, // Any 4 byte quantity
	dt_a64, // Any 8 byte quantity
	dt_i8,  // A signed 1 byte integer
	dt_i16, // A signed 2 byte integer
	dt_i32, // A signed 4 byte integer
	dt_i64, // A signed 8 byte integer
	dt_u8,  // An unsigned 1 byte integer
	dt_u16, // An unsigned 2 byte integer
	dt_u32, // An unsigned 4 byte integer
	dt_u64, // An unsigned 8 byte integer
};

// Get the data types of the immediate arguments for the given opcode.
// Use imm_count_for_opcode() to determine the minimum length of dts.
// Returns -1 on error.
int imm_types_for_opcode(int opcode, int *dts);

//
// Constants
//
enum {
	STFRAME_SIZE = 16, // Stack frame size
};

//
// Error conditions
//
enum {
	STERR_NONE = 0,
	STERR_BAD_INST, // Bad instruction
	STERR_BAD_ALIGN, // Bad alignment
	STERR_BAD_IO_ACCESS, // Access to unused IO memory
	STERR_BAD_FRAME_ACCESS, // Access to protected stack frame data
	STERR_HALT, // Processor halted
};

const char *name_for_sterr(int);
