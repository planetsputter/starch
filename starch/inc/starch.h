// starch.h
//
// Starch is a 64-bit stack-based virtual machine architecture.

#pragma once

#include <stdint.h>

enum {
	op_invalid, // Invalid operation

	//
	// Push immediate operations
	//
	op_push8as8,    // Push 1 byte imm
	op_push8asu16,  // Push 1 byte unsigned imm as 2 byte
	op_push8asu32,  // Push 1 byte unsigned imm as 4 byte
	op_push8asu64,  // Push 1 byte unsigned imm as 8 byte
	op_push8asi16,  // Push 1 byte signed imm as 2 byte
	op_push8asi32,  // Push 1 byte signed imm as 4 byte
	op_push8asi64,  // Push 1 byte signed imm as 8 byte
	op_push16as16,  // Push 2 byte imm
	op_push16asu32, // Push 2 byte unsigned imm as 4 byte
	op_push16asu64, // Push 2 byte unsigned imm as 8 byte
	op_push16asi32, // Push 2 byte signed imm as 4 byte
	op_push16asi64, // Push 2 byte signed imm as 8 byte
	op_push32as32,  // Push 4 byte imm
	op_push32asu64, // Push 4 byte unsigned imm as 8 byte
	op_push32asi64, // Push 4 byte unsigned imm as 8 byte
	op_push64as64,  // Push 8 byte imm

	//
	// Pop operations
	//
	op_pop8,  // Pop top 1 byte
	op_pop16, // Pop top 2 byte
	op_pop32, // Pop top 4 byte
	op_pop64, // Pop top 8 byte

	//
	// Duplication operations
	//
	op_dup8,  // Duplicate top 1 byte
	op_dup16, // Duplicate top 2 byte
	op_dup32, // Duplicate top 4 byte
	op_dup64, // Duplicate top 8 byte

	//
	// Setting operations
	//
	op_set8,  // Pop 1 byte and set top 1 byte
	op_set16, // Pop 2 byte and set top 2 byte
	op_set32, // Pop 4 byte and set top 4 byte
	op_set64, // Pop 8 byte and set top 8 byte

	//
	// Promotion operations
	//
	op_prom8u16,  // Promote top 1 byte to 2 byte unsigned
	op_prom8u32,  // Promote top 1 byte to 4 byte unsigned
	op_prom8u64,  // Promote top 1 byte to 8 byte unsigned
	op_prom8i16,  // Promote top 1 byte to 2 byte signed
	op_prom8i32,  // Promote top 1 byte to 4 byte signed
	op_prom8i64,  // Promote top 1 byte to 8 byte signed
	op_prom16u32, // Promote top 2 byte to 4 byte unsigned
	op_prom16u64, // Promote top 2 byte to 8 byte unsigned
	op_prom16i32, // Promote top 2 byte to 4 byte signed
	op_prom16i64, // Promote top 2 byte to 8 byte signed
	op_prom32u64, // Promote top 4 byte to 8 byte unsigned
	op_prom32i64, // Promote top 4 byte to 8 byte signed

	//
	// Demotion operations
	//
	// Note: Some demotions can be accomplished with a pop and are not included here
	op_dem64to16, // Demote top 8 byte to 2 byte
	op_dem64to8,  // Demote top 8 byte to 1 byte
	op_dem32to8,  // Demote top 4 byte to 1 byte

	//
	// Integer arithmetic operations
	//
	op_add8,    // Add 1 byte integers replacing with sum
	op_add16,   // Add 2 byte integers replacing with sum
	op_add32,   // Add 4 byte integers replacing with sum
	op_add64,   // Add 8 byte integers replacing with sum
	op_sub8,    // Subtract 1 byte integers replacing with difference
	op_sub16,   // Subtract 2 byte integers replacing with difference
	op_sub32,   // Subtract 4 byte integers replacing with difference
	op_sub64,   // Subtract 8 byte integers replacing with difference
	op_subr8,   // Subtract 1 byte integers replacing with difference
	op_subr16,  // Subtract 2 byte integers replacing with difference
	op_subr32,  // Subtract 4 byte integers replacing with difference
	op_subr64,  // Subtract 8 byte integers replacing with difference
	op_mul8,    // Multiply 1 byte integers replacing with product
	op_mul16,   // Multiply 2 byte integers replacing with product
	op_mul32,   // Multiply 4 byte integers replacing with product
	op_mul64,   // Multiply 8 byte integers replacing with product
	op_divu8,   // Divide 1 byte unsigned integers replacing with quotient
	op_divu16,  // Divide 2 byte unsigned integers replacing with quotient
	op_divu32,  // Divide 4 byte unsigned integers replacing with quotient
	op_divu64,  // Divide 8 byte unsigned integers replacing with quotient
	op_divru8,  // Divide reversed 1 byte unsigned integers replacing with quotient
	op_divru16, // Divide reversed 2 byte unsigned integers replacing with quotient
	op_divru32, // Divide reversed 4 byte unsigned integers replacing with quotient
	op_divru64, // Divide reversed 8 byte unsigned integers replacing with quotient
	op_divi8,   // Divide 1 byte signed integers replacing with quotient
	op_divi16,  // Divide 2 byte signed integers replacing with quotient
	op_divi32,  // Divide 4 byte signed integers replacing with quotient
	op_divi64,  // Divide 8 byte signed integers replacing with quotient
	op_divri8,  // Divide reversed 1 byte signed integers replacing with quotient
	op_divri16, // Divide reversed 2 byte signed integers replacing with quotient
	op_divri32, // Divide reversed 4 byte signed integers replacing with quotient
	op_divri64, // Divide reversed 8 byte signed integers replacing with quotient
	op_modu8,   // Divide 1 byte unsigned integers replacing with remainder
	op_modu16,  // Divide 2 byte unsigned integers replacing with remainder
	op_modu32,  // Divide 4 byte unsigned integers replacing with remainder
	op_modu64,  // Divide 8 byte unsigned integers replacing with remainder
	op_modru8,  // Divide reversed 1 byte unsigned integers replacing with remainder
	op_modru16, // Divide reversed 2 byte unsigned integers replacing with remainder
	op_modru32, // Divide reversed 4 byte unsigned integers replacing with remainder
	op_modru64, // Divide reversed 8 byte unsigned integers replacing with remainder
	op_modi8,   // Divide 1 byte signed integers replacing with remainder
	op_modi16,  // Divide 2 byte signed integers replacing with remainder
	op_modi32,  // Divide 4 byte signed integers replacing with remainder
	op_modi64,  // Divide 8 byte signed integers replacing with remainder
	op_modri8,  // Divide reversed 1 byte signed integers replacing with remainder
	op_modri16, // Divide reversed 2 byte signed integers replacing with remainder
	op_modri32, // Divide reversed 4 byte signed integers replacing with remainder
	op_modri64, // Divide reversed 8 byte signed integers replacing with remainder

	//
	// Bitwise shift operations
	//
	op_lshift8,   // Shift 1 byte left by top 1 byte popped
	op_lshift16,  // Shift 2 byte left by top 1 byte popped
	op_lshift32,  // Shift 4 byte left by top 1 byte popped
	op_lshift64,  // Shift 8 byte left by top 1 byte popped
	op_rshiftu8,  // Shift 1 byte unsigned left by top 1 byte popped
	op_rshiftu16, // Shift 2 byte unsigned left by top 1 byte popped
	op_rshiftu32, // Shift 4 byte unsigned left by top 1 byte popped
	op_rshiftu64, // Shift 8 byte unsigned left by top 1 byte popped
	op_rshifti8,  // Shift 1 byte signed left by top 1 byte popped
	op_rshifti16, // Shift 2 byte signed left by top 1 byte popped
	op_rshifti32, // Shift 4 byte signed left by top 1 byte popped
	op_rshifti64, // Shift 8 byte signed left by top 1 byte popped

	//
	// Bitwise logical operations
	//
	op_band8,  // Bitwise and 1 byte integers replacing with result
	op_band16, // Bitwise and 2 byte integers replacing with result
	op_band32, // Bitwise and 4 byte integers replacing with result
	op_band64, // Bitwise and 8 byte integers replacing with result
	op_bor8,   // Bitwise or 1 byte integers replacing with result
	op_bor16,  // Bitwise or 2 byte integers replacing with result
	op_bor32,  // Bitwise or 4 byte integers replacing with result
	op_bor64,  // Bitwise or 8 byte integers replacing with result
	op_bxor8,  // Bitwise xor 1 byte integers replacing with result
	op_bxor16, // Bitwise xor 2 byte integers replacing with result
	op_bxor32, // Bitwise xor 4 byte integers replacing with result
	op_bxor64, // Bitwise xor 8 byte integers replacing with result
	op_binv8,  // Bitwise invert top 1 byte
	op_binv16, // Bitwise invert top 2 byte
	op_binv32, // Bitwise invert top 4 byte
	op_binv64, // Bitwise invert top 8 byte

	//
	// Boolean logical operations
	//
	op_land8,  // Boolean and 1 byte integers replacing with result
	op_land16, // Boolean and 2 byte integers replacing with result
	op_land32, // Boolean and 4 byte integers replacing with result
	op_land64, // Boolean and 8 byte integers replacing with result
	op_lor8,   // Boolean or 1 byte integers replacing with result
	op_lor16,  // Boolean or 2 byte integers replacing with result
	op_lor32,  // Boolean or 4 byte integers replacing with result
	op_lor64,  // Boolean or 8 byte integers replacing with result
	op_linv8,  // Boolean invert top 1 byte
	op_linv16, // Boolean invert top 2 byte
	op_linv32, // Boolean invert top 4 byte
	op_linv64, // Boolean invert top 8 byte

	//
	// Comparison operations
	//
	op_ceq8,   // Push whether equal top 1 byte popped integers
	op_ceq16,  // Push whether equal top 2 byte popped integers
	op_ceq32,  // Push whether equal top 4 byte popped integers
	op_ceq64,  // Push whether equal top 8 byte popped integers
	op_cne8,   // Push whether not equal top 1 byte popped integers
	op_cne16,  // Push whether not equal top 2 byte popped integers
	op_cne32,  // Push whether not equal top 4 byte popped integers
	op_cne64,  // Push whether not equal top 8 byte popped integers
	op_cgtu8,  // Push whether top greater of top 1 byte popped unsigned integers
	op_cgtu16, // Push whether top greater of top 2 byte popped unsigned integers
	op_cgtu32, // Push whether top greater of top 4 byte popped unsigned integers
	op_cgtu64, // Push whether top greater of top 8 byte popped unsigned integers
	op_cgti8,  // Push whether top greater of top 1 byte popped signed integers
	op_cgti16, // Push whether top greater of top 2 byte popped signed integers
	op_cgti32, // Push whether top greater of top 4 byte popped signed integers
	op_cgti64, // Push whether top greater of top 8 byte popped signed integers
	op_cltu8,  // Push whether top less of top 1 byte popped unsigned integers
	op_cltu16, // Push whether top less of top 2 byte popped unsigned integers
	op_cltu32, // Push whether top less of top 4 byte popped unsigned integers
	op_cltu64, // Push whether top less of top 8 byte popped unsigned integers
	op_clti8,  // Push whether top less of top 1 byte popped signed integers
	op_clti16, // Push whether top less of top 2 byte popped signed integers
	op_clti32, // Push whether top less of top 4 byte popped signed integers
	op_clti64, // Push whether top less of top 8 byte popped signed integers
	op_cgeu8,  // Push whether top greater or equal of top 1 byte popped unsigned integers
	op_cgeu16, // Push whether top greater or equal of top 2 byte popped unsigned integers
	op_cgeu32, // Push whether top greater or equal of top 4 byte popped unsigned integers
	op_cgeu64, // Push whether top greater or equal of top 8 byte popped unsigned integers
	op_cgei8,  // Push whether top greater or equal of top 1 byte popped signed integers
	op_cgei16, // Push whether top greater or equal of top 2 byte popped signed integers
	op_cgei32, // Push whether top greater or equal of top 4 byte popped signed integers
	op_cgei64, // Push whether top greater or equal of top 8 byte popped signed integers
	op_cleu8,  // Push whether top less or equal of top 1 byte popped unsigned integers
	op_cleu16, // Push whether top less or equal of top 2 byte popped unsigned integers
	op_cleu32, // Push whether top less or equal of top 4 byte popped unsigned integers
	op_cleu64, // Push whether top less or equal of top 8 byte popped unsigned integers
	op_clei8,  // Push whether top less or equal of top 1 byte popped signed integers
	op_clei16, // Push whether top less or equal of top 2 byte popped signed integers
	op_clei32, // Push whether top less or equal of top 4 byte popped signed integers
	op_clei64, // Push whether top less or equal of top 8 byte popped signed integers

	//
	// Function operations
	//
	op_call,  // Call function at 8 byte imm addr
	op_calls, // Call function at top 8 byte popped addr
	op_ret,   // Return from current function

	//
	// Jump operations
	//
	op_jmp,     // Jump to 8 byte imm addr
	op_jmps,    // Jump to top 8 byte popped addr
	op_rjmpi8,  // Relative jump by signed 1 byte imm integer
	op_rjmpi16, // Relative jump by signed 2 byte imm integer
	op_rjmpi32, // Relative jump by signed 4 byte imm integer

	//
	// Branching operations
	//
	op_brz8,      // Jump to 8 byte imm addr if top 1 byte popped is zero
	op_brz16,     // Jump to 8 byte imm addr if top 2 byte popped is zero
	op_brz32,     // Jump to 8 byte imm addr if top 4 byte popped is zero
	op_brz64,     // Jump to 8 byte imm addr if top 8 byte popped is zero
	op_rbrz8i8,   // Relative jump by signed 1 byte imm addr if top 1 byte popped is zero
	op_rbrz8i16,  // Relative jump by signed 2 byte imm addr if top 1 byte popped is zero
	op_rbrz8i32,  // Relative jump by signed 4 byte imm addr if top 1 byte popped is zero
	op_rbrz16i8,  // Relative jump by signed 1 byte imm addr if top 2 byte popped is zero
	op_rbrz16i16, // Relative jump by signed 2 byte imm addr if top 2 byte popped is zero
	op_rbrz16i32, // Relative jump by signed 4 byte imm addr if top 2 byte popped is zero
	op_rbrz32i8,  // Relative jump by signed 1 byte imm addr if top 4 byte popped is zero
	op_rbrz32i16, // Relative jump by signed 2 byte imm addr if top 4 byte popped is zero
	op_rbrz32i32, // Relative jump by signed 4 byte imm addr if top 4 byte popped is zero
	op_rbrz64i8,  // Relative jump by signed 1 byte imm addr if top 8 byte popped is zero
	op_rbrz64i16, // Relative jump by signed 2 byte imm addr if top 8 byte popped is zero
	op_rbrz64i32, // Relative jump by signed 4 byte imm addr if top 8 byte popped is zero

	//
	// Memory operations
	//
	op_load8,          // Push 1 byte from top 8 byte address
	op_load16,         // Push 2 byte from top 8 byte address
	op_load32,         // Push 4 byte from top 8 byte address
	op_load64,         // Push 8 byte from top 8 byte address
	op_loadpop8,       // Push 1 byte from top 8 byte popped address
	op_loadpop16,      // Push 2 byte from top 8 byte popped address
	op_loadpop32,      // Push 4 byte from top 8 byte popped address
	op_loadpop64,      // Push 8 byte from top 8 byte popped address
	op_loadsfp8,       // Push 1 byte from top 8 byte signed offset from SFP
	op_loadsfp16,      // Push 2 byte from top 8 byte signed offset from SFP
	op_loadsfp32,      // Push 4 byte from top 8 byte signed offset from SFP
	op_loadsfp64,      // Push 8 byte from top 8 byte signed offset from SFP
	op_loadpopsfp8,    // Push 1 byte from top 8 byte popped signed offset from SFP
	op_loadpopsfp16,   // Push 2 byte from top 8 byte popped signed offset from SFP
	op_loadpopsfp32,   // Push 4 byte from top 8 byte popped signed offset from SFP
	op_loadpopsfp64,   // Push 8 byte from top 8 byte popped signed offset from SFP
	op_store8,         // Store top 1 byte to 8 byte address below
	op_store16,        // Store top 2 byte to 8 byte address below
	op_store32,        // Store top 4 byte to 8 byte address below
	op_store64,        // Store top 8 byte to 8 byte address below
	op_storepop8,      // Store popped 1 byte to 8 byte address below
	op_storepop16,     // Store popped 2 byte to 8 byte address below
	op_storepop32,     // Store popped 4 byte to 8 byte address below
	op_storepop64,     // Store popped 8 byte to 8 byte address below
	op_storesfp8,      // Store top 1 byte to 8 byte signed offset from SFP below
	op_storesfp16,     // Store top 2 byte to 8 byte signed offset from SFP below
	op_storesfp32,     // Store top 4 byte to 8 byte signed offset from SFP below
	op_storesfp64,     // Store top 8 byte to 8 byte signed offset from SFP below
	op_storepopsfp8,   // Store popped 1 byte to 8 byte signed offset from SFP below
	op_storepopsfp16,  // Store popped 2 byte to 8 byte signed offset from SFP below
	op_storepopsfp32,  // Store popped 4 byte to 8 byte signed offset from SFP below
	op_storepopsfp64,  // Store popped 8 byte to 8 byte signed offset from SFP below
	op_storer8,        // Store 1 byte to top 8 byte address
	op_storer16,       // Store 2 byte to top 8 byte address
	op_storer32,       // Store 4 byte to top 8 byte address
	op_storer64,       // Store 8 byte to top 8 byte address
	op_storerpop8,     // Store 1 byte to top 8 byte popped address
	op_storerpop16,    // Store 2 byte to top 8 byte popped address
	op_storerpop32,    // Store 4 byte to top 8 byte popped address
	op_storerpop64,    // Store 8 byte to top 8 byte popped address
	op_storersfp8,     // Store 1 byte to top 8 byte signed offset from SFP
	op_storersfp16,    // Store 2 byte to top 8 byte signed offset from SFP
	op_storersfp32,    // Store 4 byte to top 8 byte signed offset from SFP
	op_storersfp64,    // Store 8 byte to top 8 byte signed offset from SFP
	op_storerpopsfp8,  // Store 1 byte to top 8 byte popped signed offset from SFP
	op_storerpopsfp16, // Store 2 byte to top 8 byte popped signed offset from SFP
	op_storerpopsfp32, // Store 4 byte to top 8 byte popped signed offset from SFP
	op_storerpopsfp64, // Store 8 byte to top 8 byte popped signed offset from SFP

	//
	// Special Operations
	//
	op_setsbp, // Set SBP 8 byte imm
	op_setsfp, // Set SFP to 8 byte imm
	op_setsp,  // Set SP to 8 byte imm
	op_setslp, // Set SLP to 8 byte imm
	op_halt,   // Halts the processor with 1 byte unsigned imm exit code
	op_ext,    // Introduces an extended operation
	op_nop,    // No op
};

// Returns the name of the given opcode, or NULL for an invalid opcode
const char *name_for_opcode(int opcode);

// Returns the opcode for the given name, or -1 on error
int opcode_for_name(const char*);

// Sets *jmp_br to whether the given opcode is a jump or branch opcode, that is,
// one which may transfer control flow to a non-sequential instruction.
// Sets *delta to whether the opcode accepts a relative immediate argument.
void opcode_is_jmp_br(int opcode, int *jmp_br, int *delta);

//
// Starch data types
//
enum {
	SDT_VOID,
	SDT_A8,  // Any 1 byte quantity
	SDT_U8,  // An unsigned 1 byte integer
	SDT_I8,  // A signed 1 byte integer
	SDT_A16, // Any 2 byte quantity
	SDT_U16, // An unsigned 2 byte integer
	SDT_I16, // A signed 2 byte integer
	SDT_A32, // Any 4 byte quantity
	SDT_U32, // An unsigned 4 byte integer
	SDT_I32, // A signed 4 byte integer
	SDT_A64, // Any 8 byte quantity
	SDT_U64, // An unsigned 8 byte integer
	SDT_I64, // A signed 8 byte integer
	SDT_COUNT,
};

// Sets the minimum (inclusive) and maximum (inclusive) values for the given data type
void sdt_min_max(int sdt, int64_t *min, int64_t *max);

// Returns the byte size of the given data type, or negative on error
int sdt_size(int sdt);

// Returns the data type of the immediate argument for the given opcode.
// Returns negative on failure.
int imm_type_for_opcode(int opcode);

//
// Interrupt numbers
//
enum {
	STINT_NONE = 0, // No interrupt
	STINT_BAD_INST, // Bad instruction
	STINT_ASSERT_FAILURE, // IO assertion failed
	STINT_DIV_BY_ZERO, // Division by zero
	STINT_BAD_ALIGN, // Bad alignment
	STINT_BAD_IO_ACCESS, // Access to unused IO memory
	STINT_BAD_FRAME_ACCESS, // Access to protected stack frame data
	STINT_BAD_STACK_ACCESS, // Stack access out of stack memory region
	STINT_BAD_ADDR, // Address out of range
	STINT_NUM_INTS,
};

//
// Core memory map
//
enum {
	// Page 0 (0x0000-0x0fff) is reserved to help detect unintentional NULL pointer dereferences

	// Page 1 (0x1000-0x1fff) is IO memory which performs various special functions
	IO_STDIN_ADDR  = 0x1000,
	IO_STDOUT_ADDR = 0x1001,
	IO_FLUSH_ADDR  = 0x1002,
	IO_URAND_ADDR  = 0x1003,
	IO_ASSERT_ADDR = 0x100b,
	END_IO_ADDR    = 0x2000,

	// Page 2 (0x2000-0x2fff) contains 256 16-byte interrupt instruction sections
	BEGIN_INT_ADDR = 0x2000,

	// Page 3 is the start of program code
	INIT_PC_VAL    = 0x3000, // Initial PC
};

// Returns a string name for the given interrupt number
const char *name_for_stint(int);
