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
	op_push8as8,    // Push 8 bit imm
	op_push8asu16,  // Push 8 bit unsigned imm as 16 bit
	op_push8asu32,  // Push 8 bit unsigned imm as 32 bit
	op_push8asu64,  // Push 8 bit unsigned imm as 64 bit
	op_push8asi16,  // Push 8 bit signed imm as 16 bit
	op_push8asi32,  // Push 8 bit signed imm as 32 bit
	op_push8asi64,  // Push 8 bit signed imm as 64 bit
	op_push16as16,  // Push 16 bit imm
	op_push16asu32, // Push 16 bit unsigned imm as 32 bit
	op_push16asu64, // Push 16 bit unsigned imm as 64 bit
	op_push16asi32, // Push 16 bit signed imm as 32 bit
	op_push16asi64, // Push 16 bit signed imm as 64 bit
	op_push32as32,  // Push 32 bit imm
	op_push32asu64, // Push 32 bit unsigned imm as 64 bit
	op_push32asi64, // Push 32 bit unsigned imm as 64 bit
	op_push64as64,  // Push 64 bit imm

	//
	// Pop operations
	//
	op_pop8,  // Pop top 8 bit
	op_pop16, // Pop top 16 bit
	op_pop32, // Pop top 32 bit
	op_pop64, // Pop top 64 bit
	op_popn,  // Pop by top 64-bit signed integer

	//
	// Duplication operations
	//
	op_dup8,  // Duplicate top 8 bit
	op_dup16, // Duplicate top 16 bit
	op_dup32, // Duplicate top 32 bit
	op_dup64, // Duplicate top 64 bit

	//
	// Setting operations
	//
	op_set8,  // Pop 8 bit and set top 8 bit
	op_set16, // Pop 16 bit and set top 16 bit
	op_set32, // Pop 32 bit and set top 32 bit
	op_set64, // Pop 64 bit and set top 64 bit

	//
	// Promotion operations
	//
	// Note: Some promotions can be accomplished by pushing bytes with value zero and are not included here
	op_prom8u32,  // Promote top 8 bit to 32 bit unsigned
	op_prom8u64,  // Promote top 8 bit to 64 bit unsigned
	op_prom8i16,  // Promote top 8 bit to 16 bit signed
	op_prom8i32,  // Promote top 8 bit to 32 bit signed
	op_prom8i64,  // Promote top 8 bit to 64 bit signed
	op_prom16u64, // Promote top 16 bit to 64 bit unsigned
	op_prom16i32, // Promote top 16 bit to 32 bit signed
	op_prom16i64, // Promote top 16 bit to 64 bit signed
	op_prom32i64, // Promote top 32 bit to 64 bit signed

	//
	// Demotion operations
	//
	// Note: Some demotions can be accomplished with a pop and are not included here
	op_dem64to16, // Demote top 64 bit to 16 bit
	op_dem64to8,  // Demote top 64 bit to 8 bit
	op_dem32to8,  // Demote top 32 bit to 8 bit

	//
	// Integer arithmetic operations
	//
	op_add8,    // Add 8 bit integers replacing with sum
	op_add16,   // Add 16 bit integers replacing with sum
	op_add32,   // Add 32 bit integers replacing with sum
	op_add64,   // Add 64 bit integers replacing with sum
	op_sub8,    // Subtract 8 bit integers replacing with difference
	op_sub16,   // Subtract 16 bit integers replacing with difference
	op_sub32,   // Subtract 32 bit integers replacing with difference
	op_sub64,   // Subtract 64 bit integers replacing with difference
	op_subr8,   // Subtract 8 bit integers replacing with difference
	op_subr16,  // Subtract 16 bit integers replacing with difference
	op_subr32,  // Subtract 32 bit integers replacing with difference
	op_subr64,  // Subtract 64 bit integers replacing with difference
	op_mul8,    // Multiply 8 bit integers replacing with product
	op_mul16,   // Multiply 16 bit integers replacing with product
	op_mul32,   // Multiply 32 bit integers replacing with product
	op_mul64,   // Multiply 64 bit integers replacing with product
	op_divu8,   // Divide 8 bit unsigned integers replacing with quotient
	op_divu16,  // Divide 16 bit unsigned integers replacing with quotient
	op_divu32,  // Divide 32 bit unsigned integers replacing with quotient
	op_divu64,  // Divide 64 bit unsigned integers replacing with quotient
	op_divru8,  // Divide reversed 8 bit unsigned integers replacing with quotient
	op_divru16, // Divide reversed 16 bit unsigned integers replacing with quotient
	op_divru32, // Divide reversed 32 bit unsigned integers replacing with quotient
	op_divru64, // Divide reversed 64 bit unsigned integers replacing with quotient
	op_divi8,   // Divide 8 bit signed integers replacing with quotient
	op_divi16,  // Divide 16 bit signed integers replacing with quotient
	op_divi32,  // Divide 32 bit signed integers replacing with quotient
	op_divi64,  // Divide 64 bit signed integers replacing with quotient
	op_divri8,  // Divide reversed 8 bit signed integers replacing with quotient
	op_divri16, // Divide reversed 16 bit signed integers replacing with quotient
	op_divri32, // Divide reversed 32 bit signed integers replacing with quotient
	op_divri64, // Divide reversed 64 bit signed integers replacing with quotient
	op_modu8,   // Divide 8 bit unsigned integers replacing with remainder
	op_modu16,  // Divide 16 bit unsigned integers replacing with remainder
	op_modu32,  // Divide 32 bit unsigned integers replacing with remainder
	op_modu64,  // Divide 64 bit unsigned integers replacing with remainder
	op_modru8,  // Divide reversed 8 bit unsigned integers replacing with remainder
	op_modru16, // Divide reversed 16 bit unsigned integers replacing with remainder
	op_modru32, // Divide reversed 32 bit unsigned integers replacing with remainder
	op_modru64, // Divide reversed 64 bit unsigned integers replacing with remainder
	op_modi8,   // Divide 8 bit signed integers replacing with remainder
	op_modi16,  // Divide 16 bit signed integers replacing with remainder
	op_modi32,  // Divide 32 bit signed integers replacing with remainder
	op_modi64,  // Divide 64 bit signed integers replacing with remainder
	op_modri8,  // Divide reversed 8 bit signed integers replacing with remainder
	op_modri16, // Divide reversed 16 bit signed integers replacing with remainder
	op_modri32, // Divide reversed 32 bit signed integers replacing with remainder
	op_modri64, // Divide reversed 64 bit signed integers replacing with remainder

	//
	// Bitwise shift operations
	//
	op_lshift8,   // Shift 8 bit left by top 8 bit popped
	op_lshift16,  // Shift 16 bit left by top 8 bit popped
	op_lshift32,  // Shift 32 bit left by top 8 bit popped
	op_lshift64,  // Shift 64 bit left by top 8 bit popped
	op_rshiftu8,  // Shift 8 bit unsigned left by top 8 bit popped
	op_rshiftu16, // Shift 16 bit unsigned left by top 8 bit popped
	op_rshiftu32, // Shift 32 bit unsigned left by top 8 bit popped
	op_rshiftu64, // Shift 64 bit unsigned left by top 8 bit popped
	op_rshifti8,  // Shift 8 bit signed left by top 8 bit popped
	op_rshifti16, // Shift 16 bit signed left by top 8 bit popped
	op_rshifti32, // Shift 32 bit signed left by top 8 bit popped
	op_rshifti64, // Shift 64 bit signed left by top 8 bit popped

	//
	// Bitwise logical operations
	//
	op_band8,  // Bitwise and 8 bit integers replacing with result
	op_band16, // Bitwise and 16 bit integers replacing with result
	op_band32, // Bitwise and 32 bit integers replacing with result
	op_band64, // Bitwise and 64 bit integers replacing with result
	op_bor8,   // Bitwise or 8 bit integers replacing with result
	op_bor16,  // Bitwise or 16 bit integers replacing with result
	op_bor32,  // Bitwise or 32 bit integers replacing with result
	op_bor64,  // Bitwise or 64 bit integers replacing with result
	op_bxor8,  // Bitwise xor 8 bit integers replacing with result
	op_bxor16, // Bitwise xor 16 bit integers replacing with result
	op_bxor32, // Bitwise xor 32 bit integers replacing with result
	op_bxor64, // Bitwise xor 64 bit integers replacing with result
	op_binv8,  // Bitwise invert top 8 bit
	op_binv16, // Bitwise invert top 16 bit
	op_binv32, // Bitwise invert top 32 bit
	op_binv64, // Bitwise invert top 64 bit

	//
	// Boolean logical operations
	//
	op_land8,  // Boolean and 8 bit integers replacing with result
	op_land16, // Boolean and 16 bit integers replacing with result
	op_land32, // Boolean and 32 bit integers replacing with result
	op_land64, // Boolean and 64 bit integers replacing with result
	op_lor8,   // Boolean or 8 bit integers replacing with result
	op_lor16,  // Boolean or 16 bit integers replacing with result
	op_lor32,  // Boolean or 32 bit integers replacing with result
	op_lor64,  // Boolean or 64 bit integers replacing with result
	op_linv8,  // Boolean invert top 8 bit
	op_linv16, // Boolean invert top 16 bit
	op_linv32, // Boolean invert top 32 bit
	op_linv64, // Boolean invert top 64 bit

	//
	// Comparison operations
	//
	op_ceq8,   // Push whether equal top 8 bit popped integers
	op_ceq16,  // Push whether equal top 16 bit popped integers
	op_ceq32,  // Push whether equal top 32 bit popped integers
	op_ceq64,  // Push whether equal top 64 bit popped integers
	op_cne8,   // Push whether not equal top 8 bit popped integers
	op_cne16,  // Push whether not equal top 16 bit popped integers
	op_cne32,  // Push whether not equal top 32 bit popped integers
	op_cne64,  // Push whether not equal top 64 bit popped integers
	op_cgtu8,  // Push whether top greater of top 8 bit popped unsigned integers
	op_cgtu16, // Push whether top greater of top 16 bit popped unsigned integers
	op_cgtu32, // Push whether top greater of top 32 bit popped unsigned integers
	op_cgtu64, // Push whether top greater of top 64 bit popped unsigned integers
	op_cgti8,  // Push whether top greater of top 8 bit popped signed integers
	op_cgti16, // Push whether top greater of top 16 bit popped signed integers
	op_cgti32, // Push whether top greater of top 32 bit popped signed integers
	op_cgti64, // Push whether top greater of top 64 bit popped signed integers
	op_cltu8,  // Push whether top less of top 8 bit popped unsigned integers
	op_cltu16, // Push whether top less of top 16 bit popped unsigned integers
	op_cltu32, // Push whether top less of top 32 bit popped unsigned integers
	op_cltu64, // Push whether top less of top 64 bit popped unsigned integers
	op_clti8,  // Push whether top less of top 8 bit popped signed integers
	op_clti16, // Push whether top less of top 16 bit popped signed integers
	op_clti32, // Push whether top less of top 32 bit popped signed integers
	op_clti64, // Push whether top less of top 64 bit popped signed integers
	op_cgeu8,  // Push whether top greater or equal of top 8 bit popped unsigned integers
	op_cgeu16, // Push whether top greater or equal of top 16 bit popped unsigned integers
	op_cgeu32, // Push whether top greater or equal of top 32 bit popped unsigned integers
	op_cgeu64, // Push whether top greater or equal of top 64 bit popped unsigned integers
	op_cgei8,  // Push whether top greater or equal of top 8 bit popped signed integers
	op_cgei16, // Push whether top greater or equal of top 16 bit popped signed integers
	op_cgei32, // Push whether top greater or equal of top 32 bit popped signed integers
	op_cgei64, // Push whether top greater or equal of top 64 bit popped signed integers
	op_cleu8,  // Push whether top less or equal of top 8 bit popped unsigned integers
	op_cleu16, // Push whether top less or equal of top 16 bit popped unsigned integers
	op_cleu32, // Push whether top less or equal of top 32 bit popped unsigned integers
	op_cleu64, // Push whether top less or equal of top 64 bit popped unsigned integers
	op_clei8,  // Push whether top less or equal of top 8 bit popped signed integers
	op_clei16, // Push whether top less or equal of top 16 bit popped signed integers
	op_clei32, // Push whether top less or equal of top 32 bit popped signed integers
	op_clei64, // Push whether top less or equal of top 64 bit popped signed integers

	//
	// Function operations
	//
	op_call,  // Call function at 64 bit imm addr
	op_calls, // Call function at top 64 bit popped addr
	op_ret,   // Return from current function

	//
	// Jump operations
	//
	op_jmp,     // Jump to 64 bit imm addr
	op_jmps,    // Jump to top 64 bit popped addr
	op_rjmpi8,  // Relative jump by signed 8 bit imm integer
	op_rjmpi16, // Relative jump by signed 16 bit imm integer
	op_rjmpi32, // Relative jump by signed 32 bit imm integer

	//
	// Branching operations
	//
	op_rbrz8i8,   // Relative jump by signed 8 bit imm addr if top 8 bit popped is zero
	op_rbrz8i16,  // Relative jump by signed 16 bit imm addr if top 8 bit popped is zero
	op_rbrz8i32,  // Relative jump by signed 32 bit imm addr if top 8 bit popped is zero
	op_rbrz16i8,  // Relative jump by signed 8 bit imm addr if top 16 bit popped is zero
	op_rbrz16i16, // Relative jump by signed 16 bit imm addr if top 16 bit popped is zero
	op_rbrz16i32, // Relative jump by signed 32 bit imm addr if top 16 bit popped is zero
	op_rbrz32i8,  // Relative jump by signed 8 bit imm addr if top 32 bit popped is zero
	op_rbrz32i16, // Relative jump by signed 16 bit imm addr if top 32 bit popped is zero
	op_rbrz32i32, // Relative jump by signed 32 bit imm addr if top 32 bit popped is zero
	op_rbrz64i8,  // Relative jump by signed 8 bit imm addr if top 64 bit popped is zero
	op_rbrz64i16, // Relative jump by signed 16 bit imm addr if top 64 bit popped is zero
	op_rbrz64i32, // Relative jump by signed 32 bit imm addr if top 64 bit popped is zero

	//
	// Memory operations
	//
	op_load8,          // Push 8 bit from top 64 bit address
	op_load16,         // Push 16 bit from top 64 bit address
	op_load32,         // Push 32 bit from top 64 bit address
	op_load64,         // Push 64 bit from top 64 bit address
	op_loadpop8,       // Push 8 bit from top 64 bit popped address
	op_loadpop16,      // Push 16 bit from top 64 bit popped address
	op_loadpop32,      // Push 32 bit from top 64 bit popped address
	op_loadpop64,      // Push 64 bit from top 64 bit popped address
	op_loadsfp8,       // Push 8 bit from top 64 bit signed offset from SFP
	op_loadsfp16,      // Push 16 bit from top 64 bit signed offset from SFP
	op_loadsfp32,      // Push 32 bit from top 64 bit signed offset from SFP
	op_loadsfp64,      // Push 64 bit from top 64 bit signed offset from SFP
	op_loadpopsfp8,    // Push 8 bit from top 64 bit popped signed offset from SFP
	op_loadpopsfp16,   // Push 16 bit from top 64 bit popped signed offset from SFP
	op_loadpopsfp32,   // Push 32 bit from top 64 bit popped signed offset from SFP
	op_loadpopsfp64,   // Push 64 bit from top 64 bit popped signed offset from SFP
	op_store8,         // Store top 8 bit to 64 bit address below
	op_store16,        // Store top 16 bit to 64 bit address below
	op_store32,        // Store top 32 bit to 64 bit address below
	op_store64,        // Store top 64 bit to 64 bit address below
	op_storepop8,      // Store popped 8 bit to 64 bit address below
	op_storepop16,     // Store popped 16 bit to 64 bit address below
	op_storepop32,     // Store popped 32 bit to 64 bit address below
	op_storepop64,     // Store popped 64 bit to 64 bit address below
	op_storesfp8,      // Store top 8 bit to 64 bit signed offset from SFP below
	op_storesfp16,     // Store top 16 bit to 64 bit signed offset from SFP below
	op_storesfp32,     // Store top 32 bit to 64 bit signed offset from SFP below
	op_storesfp64,     // Store top 64 bit to 64 bit signed offset from SFP below
	op_storepopsfp8,   // Store popped 8 bit to 64 bit signed offset from SFP below
	op_storepopsfp16,  // Store popped 16 bit to 64 bit signed offset from SFP below
	op_storepopsfp32,  // Store popped 32 bit to 64 bit signed offset from SFP below
	op_storepopsfp64,  // Store popped 64 bit to 64 bit signed offset from SFP below
	op_storer8,        // Store 8 bit to top 64 bit address
	op_storer16,       // Store 16 bit to top 64 bit address
	op_storer32,       // Store 32 bit to top 64 bit address
	op_storer64,       // Store 64 bit to top 64 bit address
	op_storerpop8,     // Store 8 bit to top 64 bit popped address
	op_storerpop16,    // Store 16 bit to top 64 bit popped address
	op_storerpop32,    // Store 32 bit to top 64 bit popped address
	op_storerpop64,    // Store 64 bit to top 64 bit popped address
	op_storersfp8,     // Store 8 bit to top 64 bit signed offset from SFP
	op_storersfp16,    // Store 16 bit to top 64 bit signed offset from SFP
	op_storersfp32,    // Store 32 bit to top 64 bit signed offset from SFP
	op_storersfp64,    // Store 64 bit to top 64 bit signed offset from SFP
	op_storerpopsfp8,  // Store 8 bit to top 64 bit popped signed offset from SFP
	op_storerpopsfp16, // Store 16 bit to top 64 bit popped signed offset from SFP
	op_storerpopsfp32, // Store 32 bit to top 64 bit popped signed offset from SFP
	op_storerpopsfp64, // Store 64 bit to top 64 bit popped signed offset from SFP

	//
	// Special Operations
	//
	op_setsbp, // Set SBP 64 bit imm
	op_setsfp, // Set SFP to 64 bit imm
	op_setsp,  // Set SP to 64 bit imm
	op_setslp, // Set SLP to 64 bit imm
	op_halt,   // Halts the processor with 8 bit unsigned imm exit code
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
	SDT_A8,  // Any 8 bit quantity
	SDT_U8,  // An unsigned 8 bit integer
	SDT_I8,  // A signed 8 bit integer
	SDT_A16, // Any 16 bit quantity
	SDT_U16, // An unsigned 16 bit integer
	SDT_I16, // A signed 16 bit integer
	SDT_A32, // Any 32 bit quantity
	SDT_U32, // An unsigned 32 bit integer
	SDT_I32, // A signed 32 bit integer
	SDT_A64, // Any 64 bit quantity
	SDT_U64, // An unsigned 64 bit integer
	SDT_I64, // A signed 64 bit integer
	SDT_COUNT,
};

// Sets the minimum (inclusive) and maximum (inclusive) values for the given data type
void sdt_min_max(int sdt, int64_t *min, int64_t *max);

// Returns the byte size of the given data type, or negative on error
int sdt_size(int sdt);

// Returns the smallest signed data type to contain the given value
int sdt_icontain(int64_t val);

// Returns the smallest unsigned data type to contain the given value
int sdt_ucontain(uint64_t val);

// Returns whether (1) or not (0) the given data type contains the given value
int sdt_contains(int sdt, int64_t val);

// Returns the data type of the immediate argument for the given opcode.
// Returns negative on failure.
int imm_type_for_opcode(int opcode);

//
// Interrupt numbers
//
enum {
	STINT_NONE = 0, // No interrupt
	STINT_INVALID_INST, // Bad instruction
	STINT_ASSERT_FAILURE, // IO assertion failed
	STINT_DIV_BY_ZERO, // Division by zero
	STINT_BAD_IO_ACCESS, // Access to unused IO memory
	STINT_BAD_FRAME_ACCESS, // Stack access out of current stack frame
	STINT_BAD_STACK_ACCESS, // Stack access out of stack memory region
	STINT_BAD_ADDR, // Address out of range
	STINT_NUM_INTS,
};

// Returns the name of the given interrupt number, or NULL for an invalid interrupt number
const char *name_for_stint(int);

// Returns the interrupt number for the given interrupt name (e.g. "STINT_NONE")
// or negative on failure.
int stint_for_name(const char*);

//
// Core memory map
//
enum {
	// Page 0 (0x0000-0x0fff) is unmapped IO memory, reserved to help detect unintentional NULL pointer dereferences

	// Page 1 (0x1000-0x1fff) contains IO memory which performs various special functions
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
