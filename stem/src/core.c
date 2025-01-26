// core.c

#include <string.h>

#include "core.h"
#include "starch.h"

void core_init(struct core *core)
{
	memset(core, 0, sizeof(struct core));
	// Start with a stack of 1GiB
	// @todo: Incorporate stack size into a linkable binary format.
	core->sbp = 0x40000000;
	core->sfp = 0x40000000;
	core->sp = 0x40000000;
}

void core_destroy(struct core*)
{
}

int core_step(struct core *core, struct mem *mem)
{
	// Fetch instruction from memory
	uint8_t opcode;
	int ret = mem_read8(mem, core, core->pc, &opcode);
	if (ret) return ret;

	uint8_t temp_u8, temp_u8b;
	int8_t temp_i8, temp_i8b;
	uint16_t temp_u16, temp_u16b;
	int16_t temp_i16, temp_i16b;
	uint32_t temp_u32, temp_u32b;
	int32_t temp_i32, temp_i32b;
	uint64_t temp_addr, temp_u64, temp_u64b;
	int64_t temp_i64, temp_i64b;

	//
	// Execute instruction on core and memory
	//
	switch (opcode) {
	//
	// Invalid instruction
	//
	case op_invalid:
		return STERR_BAD_INST;

	//
	// Push immediate operations
	//
	case op_push8:
		ret = mem_read8(mem, core, core->pc + 1, &temp_u8); // Read imm
		if (ret) return ret;
		ret = mem_write8(mem, core, core->sp - 1, temp_u8); // Write to stack
		if (ret) return ret;
		core->sp -= 1;
		core->pc += 2;
		break;
	case op_push8u16:
		ret = mem_read8(mem, core, core->pc + 1, &temp_u8); // Read imm
		if (ret) return ret;
		ret = mem_write16(mem, core, core->sp - 2, temp_u8); // Write to stack
		if (ret) return ret;
		core->sp -= 2;
		core->pc += 2;
		break;
	case op_push8u32:
		ret = mem_read8(mem, core, core->pc + 1, &temp_u8); // Read imm
		if (ret) return ret;
		ret = mem_write32(mem, core, core->sp - 4, temp_u8); // Write to stack
		if (ret) return ret;
		core->sp -= 4;
		core->pc += 2;
		break;
	case op_push8u64:
		ret = mem_read8(mem, core, core->pc + 1, &temp_u8); // Read imm
		if (ret) return ret;
		ret = mem_write64(mem, core, core->sp - 8, temp_u8); // Write to stack
		if (ret) return ret;
		core->sp -= 8;
		core->pc += 2;
		break;
	case op_push8i16:
		ret = mem_read8(mem, core, core->pc + 1, &temp_u8); // Read imm
		if (ret) return ret;
		ret = mem_write16(mem, core, core->sp - 2, (int8_t)temp_u8); // Write to stack
		if (ret) return ret;
		core->sp -= 2;
		core->pc += 2;
		break;
	case op_push8i32:
		ret = mem_read8(mem, core, core->pc + 1, &temp_u8); // Read imm
		if (ret) return ret;
		ret = mem_write32(mem, core, core->sp - 4, (int8_t)temp_u8); // Write to stack
		if (ret) return ret;
		core->sp -= 4;
		core->pc += 2;
		break;
	case op_push8i64:
		ret = mem_read8(mem, core, core->pc + 1, &temp_u8); // Read imm
		if (ret) return ret;
		ret = mem_write64(mem, core, core->sp - 8, (int8_t)temp_u8); // Write to stack
		if (ret) return ret;
		core->sp -= 8;
		core->pc += 2;
		break;
	case op_push16:
		ret = mem_read16(mem, core, core->pc + 1, &temp_u16); // Read imm
		if (ret) return ret;
		ret = mem_write16(mem, core, core->sp - 2, temp_u16); // Write to stack
		if (ret) return ret;
		core->sp -= 2;
		core->pc += 3;
		break;
	case op_push16u32:
		ret = mem_read16(mem, core, core->pc + 1, &temp_u16); // Read imm
		if (ret) return ret;
		ret = mem_write32(mem, core, core->sp - 4, temp_u16); // Write to stack
		if (ret) return ret;
		core->sp -= 4;
		core->pc += 3;
		break;
	case op_push16u64:
		ret = mem_read16(mem, core, core->pc + 1, &temp_u16); // Read imm
		if (ret) return ret;
		ret = mem_write64(mem, core, core->sp - 8, temp_u16); // Write to stack
		if (ret) return ret;
		core->sp -= 8;
		core->pc += 3;
		break;
	case op_push16i32:
		ret = mem_read16(mem, core, core->pc + 1, &temp_u16); // Read imm
		if (ret) return ret;
		ret = mem_write32(mem, core, core->sp - 4, (int16_t)temp_u16); // Write to stack
		if (ret) return ret;
		core->sp -= 4;
		core->pc += 3;
		break;
	case op_push16i64:
		ret = mem_read16(mem, core, core->pc + 1, &temp_u16); // Read imm
		if (ret) return ret;
		ret = mem_write64(mem, core, core->sp - 8, (int16_t)temp_u16); // Write to stack
		if (ret) return ret;
		core->sp -= 8;
		core->pc += 3;
		break;
	case op_push32:
		ret = mem_read32(mem, core, core->pc + 1, &temp_u32); // Read imm
		if (ret) return ret;
		ret = mem_write32(mem, core, core->sp - 4, temp_u32); // Write to stack
		if (ret) return ret;
		core->sp -= 4;
		core->pc += 5;
		break;
	case op_push32u64:
		ret = mem_read32(mem, core, core->pc + 1, &temp_u32); // Read imm
		if (ret) return ret;
		ret = mem_write64(mem, core, core->sp - 8, temp_u32); // Write to stack
		if (ret) return ret;
		core->sp -= 8;
		core->pc += 5;
		break;
	case op_push32i64:
		ret = mem_read32(mem, core, core->pc + 1, &temp_u32); // Read imm
		if (ret) return ret;
		ret = mem_write64(mem, core, core->sp - 8, (int32_t)temp_u32); // Write to stack
		if (ret) return ret;
		core->sp -= 8;
		core->pc += 5;
		break;
	case op_push64:
		ret = mem_read64(mem, core, core->pc + 1, &temp_u64); // Read imm
		if (ret) return ret;
		ret = mem_write64(mem, core, core->sp - 8, temp_u64); // Write to stack
		if (ret) return ret;
		core->sp -= 8;
		core->pc += 9;
		break;

	//
	// Pop operations
	//
	case op_pop8:
		core->sp += 1;
		core->pc += 1;
		break;
	case op_pop16:
		core->sp += 2;
		core->pc += 1;
		break;
	case op_pop32:
		core->sp += 4;
		core->pc += 1;
		break;
	case op_pop64:
		core->sp += 8;
		core->pc += 1;
		break;

	//
	// Duplication operations
	//
	case op_dup8:
		ret = mem_read8(mem, core, core->sp, &temp_u8);
		if (ret) return ret;
		ret = mem_write8(mem, core, core->sp - 1, temp_u8);
		if (ret) return ret;
		core->sp -= 1;
		core->pc += 1;
		break;
	case op_dup16:
		ret = mem_read16(mem, core, core->sp, &temp_u16);
		if (ret) return ret;
		ret = mem_write16(mem, core, core->sp - 2, temp_u16);
		if (ret) return ret;
		core->sp -= 2;
		core->pc += 1;
		break;
	case op_dup32:
		ret = mem_read32(mem, core, core->sp, &temp_u32);
		if (ret) return ret;
		ret = mem_write32(mem, core, core->sp - 4, temp_u32);
		if (ret) return ret;
		core->sp -= 4;
		core->pc += 1;
		break;
	case op_dup64:
		ret = mem_read64(mem, core, core->sp, &temp_u64);
		if (ret) return ret;
		ret = mem_write64(mem, core, core->sp - 8, temp_u64);
		if (ret) return ret;
		core->sp -= 8;
		core->pc += 1;
		break;

	//
	// Setting operations
	//
	case op_set8:
		ret = mem_read8(mem, core, core->sp, &temp_u8);
		if (ret) return ret;
		ret = mem_write8(mem, core, core->sp + 1, temp_u8);
		if (ret) return ret;
		core->sp += 1;
		core->pc += 1;
		break;
	case op_set16:
		ret = mem_read16(mem, core, core->sp, &temp_u16);
		if (ret) return ret;
		ret = mem_write16(mem, core, core->sp + 2, temp_u16);
		if (ret) return ret;
		core->sp += 2;
		core->pc += 1;
		break;
	case op_set32:
		ret = mem_read32(mem, core, core->sp, &temp_u32);
		if (ret) return ret;
		ret = mem_write32(mem, core, core->sp + 4, temp_u32);
		if (ret) return ret;
		core->sp += 4;
		core->pc += 1;
		break;
	case op_set64:
		ret = mem_read64(mem, core, core->sp, &temp_u64);
		if (ret) return ret;
		ret = mem_write64(mem, core, core->sp + 8, temp_u64);
		if (ret) return ret;
		core->sp += 8;
		core->pc += 1;
		break;

	//
	// Promotion operations
	//
	case op_prom8u16:
		ret = mem_read8(mem, core, core->sp, &temp_u8);
		if (ret) return ret;
		ret = mem_write16(mem, core, core->sp - 1, temp_u8);
		if (ret) return ret;
		core->sp -= 1;
		core->pc += 1;
		break;
	case op_prom8u32:
		ret = mem_read8(mem, core, core->sp, &temp_u8);
		if (ret) return ret;
		ret = mem_write32(mem, core, core->sp - 3, temp_u8);
		if (ret) return ret;
		core->sp -= 3;
		core->pc += 1;
		break;
	case op_prom8u64:
		ret = mem_read8(mem, core, core->sp, &temp_u8);
		if (ret) return ret;
		ret = mem_write64(mem, core, core->sp - 7, temp_u8);
		if (ret) return ret;
		core->sp -= 7;
		core->pc += 1;
		break;
	case op_prom8i16:
		ret = mem_read8(mem, core, core->sp, &temp_u8);
		if (ret) return ret;
		ret = mem_write16(mem, core, core->sp - 1, (int8_t)temp_u8);
		if (ret) return ret;
		core->sp -= 1;
		core->pc += 1;
		break;
	case op_prom8i32:
		ret = mem_read8(mem, core, core->sp, &temp_u8);
		if (ret) return ret;
		ret = mem_write32(mem, core, core->sp - 3, (int8_t)temp_u8);
		if (ret) return ret;
		core->sp -= 3;
		core->pc += 1;
		break;
	case op_prom8i64:
		ret = mem_read8(mem, core, core->sp, &temp_u8);
		if (ret) return ret;
		ret = mem_write64(mem, core, core->sp - 7, (int8_t)temp_u8);
		if (ret) return ret;
		core->sp -= 7;
		core->pc += 1;
		break;
	case op_prom16u32:
		ret = mem_read16(mem, core, core->sp, &temp_u16);
		if (ret) return ret;
		ret = mem_write32(mem, core, core->sp - 2, temp_u16);
		if (ret) return ret;
		core->sp -= 2;
		core->pc += 1;
		break;
	case op_prom16u64:
		ret = mem_read16(mem, core, core->sp, &temp_u16);
		if (ret) return ret;
		ret = mem_write64(mem, core, core->sp - 6, temp_u16);
		if (ret) return ret;
		core->sp -= 6;
		core->pc += 1;
		break;
	case op_prom16i32:
		ret = mem_read16(mem, core, core->sp, &temp_u16);
		if (ret) return ret;
		ret = mem_write32(mem, core, core->sp - 2, (int16_t)temp_u16);
		if (ret) return ret;
		core->sp -= 2;
		core->pc += 1;
		break;
	case op_prom16i64:
		ret = mem_read16(mem, core, core->sp, &temp_u16);
		if (ret) return ret;
		ret = mem_write64(mem, core, core->sp - 6, (int16_t)temp_u16);
		if (ret) return ret;
		core->sp -= 6;
		core->pc += 1;
		break;
	case op_prom32u64:
		ret = mem_read32(mem, core, core->sp, &temp_u32);
		if (ret) return ret;
		ret = mem_write64(mem, core, core->sp - 4, temp_u32);
		if (ret) return ret;
		core->sp -= 4;
		core->pc += 1;
		break;
	case op_prom32i64:
		ret = mem_read32(mem, core, core->sp, &temp_u32);
		if (ret) return ret;
		ret = mem_write64(mem, core, core->sp - 4, (int32_t)temp_u32);
		if (ret) return ret;
		core->sp -= 4;
		core->pc += 1;
		break;

	//
	// Demotion operations
	//
	case op_dem64to16:
		core->sp += 6;
		core->pc += 1;
		break;
	case op_dem64to8:
		core->sp += 7;
		core->pc += 1;
		break;
	case op_dem32to8:
		core->sp += 3;
		core->pc += 1;
		break;

	//
	// Integer arithmetic operations
	//
	case op_add8:
		ret = mem_read8(mem, core, core->sp, &temp_u8); // Read operand
		if (ret) return ret;
		ret = mem_read8(mem, core, core->sp + 1, &temp_u8b); // Read operand
		if (ret) return ret;
		ret = mem_write8(mem, core, core->sp + 1, temp_u8 + temp_u8b); // Write sum
		if (ret) return ret;
		core->sp += 1;
		core->pc += 1;
		break;
	case op_add16:
		ret = mem_read16(mem, core, core->sp, &temp_u16); // Read operand
		if (ret) return ret;
		ret = mem_read16(mem, core, core->sp + 2, &temp_u16b); // Read operand
		if (ret) return ret;
		ret = mem_write16(mem, core, core->sp + 2, temp_u16 + temp_u16b); // Write sum
		if (ret) return ret;
		core->sp += 2;
		core->pc += 1;
		break;
	case op_add32:
		ret = mem_read32(mem, core, core->sp, &temp_u32); // Read operand
		if (ret) return ret;
		ret = mem_read32(mem, core, core->sp + 4, &temp_u32b); // Read operand
		if (ret) return ret;
		ret = mem_write32(mem, core, core->sp + 4, temp_u32 + temp_u32b); // Write sum
		if (ret) return ret;
		core->sp += 4;
		core->pc += 1;
		break;
	case op_add64:
		ret = mem_read64(mem, core, core->sp, &temp_u64); // Read operand
		if (ret) return ret;
		ret = mem_read64(mem, core, core->sp + 8, &temp_u64b); // Read operand
		if (ret) return ret;
		ret = mem_write64(mem, core, core->sp + 8, temp_u64 + temp_u64b); // Write sum
		if (ret) return ret;
		core->sp += 8;
		core->pc += 1;
		break;
	case op_sub8:
		ret = mem_read8(mem, core, core->sp, &temp_u8); // Read operand
		if (ret) return ret;
		ret = mem_read8(mem, core, core->sp + 1, &temp_u8b); // Read operand
		if (ret) return ret;
		ret = mem_write8(mem, core, core->sp + 1, temp_u8 - temp_u8b); // Write difference
		if (ret) return ret;
		core->sp += 1;
		core->pc += 1;
		break;
	case op_sub16:
		ret = mem_read16(mem, core, core->sp, &temp_u16); // Read operand
		if (ret) return ret;
		ret = mem_read16(mem, core, core->sp + 2, &temp_u16b); // Read operand
		if (ret) return ret;
		ret = mem_write16(mem, core, core->sp + 2, temp_u16 - temp_u16b); // Write difference
		if (ret) return ret;
		core->sp += 2;
		core->pc += 1;
		break;
	case op_sub32:
		ret = mem_read32(mem, core, core->sp, &temp_u32); // Read operand
		if (ret) return ret;
		ret = mem_read32(mem, core, core->sp + 4, &temp_u32b); // Read operand
		if (ret) return ret;
		ret = mem_write32(mem, core, core->sp + 4, temp_u32 - temp_u32b); // Write difference
		if (ret) return ret;
		core->sp += 4;
		core->pc += 1;
		break;
	case op_sub64:
		ret = mem_read64(mem, core, core->sp, &temp_u64); // Read operand
		if (ret) return ret;
		ret = mem_read64(mem, core, core->sp + 8, &temp_u64b); // Read operand
		if (ret) return ret;
		ret = mem_write64(mem, core, core->sp + 8, temp_u64 - temp_u64b); // Write difference
		if (ret) return ret;
		core->sp += 8;
		core->pc += 1;
		break;
	case op_subr8:
		ret = mem_read8(mem, core, core->sp, &temp_u8); // Read operand
		if (ret) return ret;
		ret = mem_read8(mem, core, core->sp + 1, &temp_u8b); // Read operand
		if (ret) return ret;
		ret = mem_write8(mem, core, core->sp + 1, temp_u8b - temp_u8); // Write difference
		if (ret) return ret;
		core->sp += 1;
		core->pc += 1;
		break;
	case op_subr16:
		ret = mem_read16(mem, core, core->sp, &temp_u16); // Read operand
		if (ret) return ret;
		ret = mem_read16(mem, core, core->sp + 2, &temp_u16b); // Read operand
		if (ret) return ret;
		ret = mem_write16(mem, core, core->sp + 2, temp_u16b - temp_u16); // Write difference
		if (ret) return ret;
		core->sp += 2;
		core->pc += 1;
		break;
	case op_subr32:
		ret = mem_read32(mem, core, core->sp, &temp_u32); // Read operand
		if (ret) return ret;
		ret = mem_read32(mem, core, core->sp + 4, &temp_u32b); // Read operand
		if (ret) return ret;
		ret = mem_write32(mem, core, core->sp + 4, temp_u32b - temp_u32); // Write difference
		if (ret) return ret;
		core->sp += 4;
		core->pc += 1;
		break;
	case op_subr64:
		ret = mem_read64(mem, core, core->sp, &temp_u64); // Read operand
		if (ret) return ret;
		ret = mem_read64(mem, core, core->sp + 8, &temp_u64b); // Read operand
		if (ret) return ret;
		ret = mem_write64(mem, core, core->sp + 8, temp_u64b - temp_u64); // Write difference
		if (ret) return ret;
		core->sp += 8;
		core->pc += 1;
		break;
	case op_mulu8:
		ret = mem_read8(mem, core, core->sp, &temp_u8); // Read operand
		if (ret) return ret;
		ret = mem_read8(mem, core, core->sp + 1, &temp_u8b); // Read operand
		if (ret) return ret;
		ret = mem_write8(mem, core, core->sp + 1, temp_u8 * temp_u8b); // Write product
		if (ret) return ret;
		core->sp += 1;
		core->pc += 1;
		break;
	case op_mulu16:
		ret = mem_read16(mem, core, core->sp, &temp_u16); // Read operand
		if (ret) return ret;
		ret = mem_read16(mem, core, core->sp + 2, &temp_u16b); // Read operand
		if (ret) return ret;
		ret = mem_write16(mem, core, core->sp + 2, temp_u16 * temp_u16b); // Write product
		if (ret) return ret;
		core->sp += 2;
		core->pc += 1;
		break;
	case op_mulu32:
		ret = mem_read32(mem, core, core->sp, &temp_u32); // Read operand
		if (ret) return ret;
		ret = mem_read32(mem, core, core->sp + 4, &temp_u32b); // Read operand
		if (ret) return ret;
		ret = mem_write32(mem, core, core->sp + 4, temp_u32 * temp_u32b); // Write product
		if (ret) return ret;
		core->sp += 4;
		core->pc += 1;
		break;
	case op_mulu64:
		ret = mem_read64(mem, core, core->sp, &temp_u64); // Read operand
		if (ret) return ret;
		ret = mem_read64(mem, core, core->sp + 8, &temp_u64b); // Read operand
		if (ret) return ret;
		ret = mem_write64(mem, core, core->sp + 8, temp_u64 * temp_u64b); // Write product
		if (ret) return ret;
		core->sp += 8;
		core->pc += 1;
		break;
	case op_muli8:
		ret = mem_read8(mem, core, core->sp, (uint8_t*)&temp_i8); // Read operand
		if (ret) return ret;
		ret = mem_read8(mem, core, core->sp + 1, (uint8_t*)&temp_i8b); // Read operand
		if (ret) return ret;
		ret = mem_write8(mem, core, core->sp + 1, temp_i8 * temp_i8b); // Write product
		if (ret) return ret;
		core->sp += 1;
		core->pc += 1;
		break;
	case op_muli16:
		ret = mem_read16(mem, core, core->sp, (uint16_t*)&temp_i16); // Read operand
		if (ret) return ret;
		ret = mem_read16(mem, core, core->sp + 2, (uint16_t*)&temp_i16b); // Read operand
		if (ret) return ret;
		ret = mem_write16(mem, core, core->sp + 2, temp_i16 * temp_i16b); // Write product
		if (ret) return ret;
		core->sp += 2;
		core->pc += 1;
		break;
	case op_muli32:
		ret = mem_read32(mem, core, core->sp, (uint32_t*)&temp_i32); // Read operand
		if (ret) return ret;
		ret = mem_read32(mem, core, core->sp + 4, (uint32_t*)&temp_i32b); // Read operand
		if (ret) return ret;
		ret = mem_write32(mem, core, core->sp + 4, temp_i32 * temp_i32b); // Write product
		if (ret) return ret;
		core->sp += 4;
		core->pc += 1;
		break;
	case op_muli64:
		ret = mem_read64(mem, core, core->sp, (uint64_t*)&temp_i64); // Read operand
		if (ret) return ret;
		ret = mem_read64(mem, core, core->sp + 8, (uint64_t*)&temp_i64b); // Read operand
		if (ret) return ret;
		ret = mem_write64(mem, core, core->sp + 8, temp_i64 * temp_i64b); // Write product
		if (ret) return ret;
		core->sp += 8;
		core->pc += 1;
		break;
	case op_divu8:
		ret = mem_read8(mem, core, core->sp, &temp_u8); // Read operand
		if (ret) return ret;
		ret = mem_read8(mem, core, core->sp + 1, &temp_u8b); // Read operand
		if (ret) return ret;
		ret = mem_write8(mem, core, core->sp + 1, temp_u8 / temp_u8b); // Write quotient
		if (ret) return ret;
		core->sp += 1;
		core->pc += 1;
		break;
	case op_divu16:
		ret = mem_read16(mem, core, core->sp, &temp_u16); // Read operand
		if (ret) return ret;
		ret = mem_read16(mem, core, core->sp + 2, &temp_u16b); // Read operand
		if (ret) return ret;
		ret = mem_write16(mem, core, core->sp + 2, temp_u16 / temp_u16b); // Write quotient
		if (ret) return ret;
		core->sp += 2;
		core->pc += 1;
		break;
	case op_divu32:
		ret = mem_read32(mem, core, core->sp, &temp_u32); // Read operand
		if (ret) return ret;
		ret = mem_read32(mem, core, core->sp + 4, &temp_u32b); // Read operand
		if (ret) return ret;
		ret = mem_write32(mem, core, core->sp + 4, temp_u32 / temp_u32b); // Write quotient
		if (ret) return ret;
		core->sp += 4;
		core->pc += 1;
		break;
	case op_divu64:
		ret = mem_read64(mem, core, core->sp, &temp_u64); // Read operand
		if (ret) return ret;
		ret = mem_read64(mem, core, core->sp + 8, &temp_u64b); // Read operand
		if (ret) return ret;
		ret = mem_write64(mem, core, core->sp + 8, temp_u64 / temp_u64b); // Write quotient
		if (ret) return ret;
		core->sp += 8;
		core->pc += 1;
		break;
	case op_divru8:
		ret = mem_read8(mem, core, core->sp, &temp_u8); // Read operand
		if (ret) return ret;
		ret = mem_read8(mem, core, core->sp + 1, &temp_u8b); // Read operand
		if (ret) return ret;
		ret = mem_write8(mem, core, core->sp + 1, temp_u8b / temp_u8); // Write quotient
		if (ret) return ret;
		core->sp += 1;
		core->pc += 1;
		break;
	case op_divru16:
		ret = mem_read16(mem, core, core->sp, &temp_u16); // Read operand
		if (ret) return ret;
		ret = mem_read16(mem, core, core->sp + 2, &temp_u16b); // Read operand
		if (ret) return ret;
		ret = mem_write16(mem, core, core->sp + 2, temp_u16b / temp_u16); // Write quotient
		if (ret) return ret;
		core->sp += 2;
		core->pc += 1;
		break;
	case op_divru32:
		ret = mem_read32(mem, core, core->sp, &temp_u32); // Read operand
		if (ret) return ret;
		ret = mem_read32(mem, core, core->sp + 4, &temp_u32b); // Read operand
		if (ret) return ret;
		ret = mem_write32(mem, core, core->sp + 4, temp_u32b / temp_u32); // Write quotient
		if (ret) return ret;
		core->sp += 4;
		core->pc += 1;
		break;
	case op_divru64:
		ret = mem_read64(mem, core, core->sp, &temp_u64); // Read operand
		if (ret) return ret;
		ret = mem_read64(mem, core, core->sp + 8, &temp_u64b); // Read operand
		if (ret) return ret;
		ret = mem_write64(mem, core, core->sp + 8, temp_u64b / temp_u64); // Write quotient
		if (ret) return ret;
		core->sp += 8;
		core->pc += 1;
		break;
	case op_divi8:
		ret = mem_read8(mem, core, core->sp, (uint8_t*)&temp_i8); // Read operand
		if (ret) return ret;
		ret = mem_read8(mem, core, core->sp + 1, (uint8_t*)&temp_i8b); // Read operand
		if (ret) return ret;
		ret = mem_write8(mem, core, core->sp + 1, temp_i8 / temp_i8b); // Write quotient
		if (ret) return ret;
		core->sp += 1;
		core->pc += 1;
		break;
	case op_divi16:
		ret = mem_read16(mem, core, core->sp, (uint16_t*)&temp_i16); // Read operand
		if (ret) return ret;
		ret = mem_read16(mem, core, core->sp + 2, (uint16_t*)&temp_i16b); // Read operand
		if (ret) return ret;
		ret = mem_write16(mem, core, core->sp + 2, temp_i16 / temp_i16b); // Write quotient
		if (ret) return ret;
		core->sp += 2;
		core->pc += 1;
		break;
	case op_divi32:
		ret = mem_read32(mem, core, core->sp, (uint32_t*)&temp_i32); // Read operand
		if (ret) return ret;
		ret = mem_read32(mem, core, core->sp + 4, (uint32_t*)&temp_i32b); // Read operand
		if (ret) return ret;
		ret = mem_write32(mem, core, core->sp + 4, temp_i32 / temp_i32b); // Write quotient
		if (ret) return ret;
		core->sp += 4;
		core->pc += 1;
		break;
	case op_divi64:
		ret = mem_read64(mem, core, core->sp, (uint64_t*)&temp_i64); // Read operand
		if (ret) return ret;
		ret = mem_read64(mem, core, core->sp + 8, (uint64_t*)&temp_i64b); // Read operand
		if (ret) return ret;
		ret = mem_write64(mem, core, core->sp + 8, temp_i64 / temp_i64b); // Write quotient
		if (ret) return ret;
		core->sp += 8;
		core->pc += 1;
		break;
	case op_divri8:
		ret = mem_read8(mem, core, core->sp, (uint8_t*)&temp_i8); // Read operand
		if (ret) return ret;
		ret = mem_read8(mem, core, core->sp + 1, (uint8_t*)&temp_i8b); // Read operand
		if (ret) return ret;
		ret = mem_write8(mem, core, core->sp + 1, temp_i8b / temp_i8); // Write quotient
		if (ret) return ret;
		core->sp += 1;
		core->pc += 1;
		break;
	case op_divri16:
		ret = mem_read16(mem, core, core->sp, (uint16_t*)&temp_i16); // Read operand
		if (ret) return ret;
		ret = mem_read16(mem, core, core->sp + 2, (uint16_t*)&temp_i16b); // Read operand
		if (ret) return ret;
		ret = mem_write16(mem, core, core->sp + 2, temp_i16b / temp_i16); // Write quotient
		if (ret) return ret;
		core->sp += 2;
		core->pc += 1;
		break;
	case op_divri32:
		ret = mem_read32(mem, core, core->sp, (uint32_t*)&temp_i32); // Read operand
		if (ret) return ret;
		ret = mem_read32(mem, core, core->sp + 4, (uint32_t*)&temp_i32b); // Read operand
		if (ret) return ret;
		ret = mem_write32(mem, core, core->sp + 4, temp_i32b / temp_i32); // Write quotient
		if (ret) return ret;
		core->sp += 4;
		core->pc += 1;
		break;
	case op_divri64:
		ret = mem_read64(mem, core, core->sp, (uint64_t*)&temp_i64); // Read operand
		if (ret) return ret;
		ret = mem_read64(mem, core, core->sp + 8, (uint64_t*)&temp_i64b); // Read operand
		if (ret) return ret;
		ret = mem_write64(mem, core, core->sp + 8, temp_i64b / temp_i64); // Write quotient
		if (ret) return ret;
		core->sp += 8;
		core->pc += 1;
		break;
	case op_modu8:
		ret = mem_read8(mem, core, core->sp, &temp_u8); // Read operand
		if (ret) return ret;
		ret = mem_read8(mem, core, core->sp + 1, &temp_u8b); // Read operand
		if (ret) return ret;
		ret = mem_write8(mem, core, core->sp + 1, temp_u8 % temp_u8b); // Write remainder
		if (ret) return ret;
		core->sp += 1;
		core->pc += 1;
		break;
	case op_modu16:
		ret = mem_read16(mem, core, core->sp, &temp_u16); // Read operand
		if (ret) return ret;
		ret = mem_read16(mem, core, core->sp + 2, &temp_u16b); // Read operand
		if (ret) return ret;
		ret = mem_write16(mem, core, core->sp + 2, temp_u16 % temp_u16b); // Write remainder
		if (ret) return ret;
		core->sp += 2;
		core->pc += 1;
		break;
	case op_modu32:
		ret = mem_read32(mem, core, core->sp, &temp_u32); // Read operand
		if (ret) return ret;
		ret = mem_read32(mem, core, core->sp + 4, &temp_u32b); // Read operand
		if (ret) return ret;
		ret = mem_write32(mem, core, core->sp + 4, temp_u32 % temp_u32b); // Write remainder
		if (ret) return ret;
		core->sp += 4;
		core->pc += 1;
		break;
	case op_modu64:
		ret = mem_read64(mem, core, core->sp, &temp_u64); // Read operand
		if (ret) return ret;
		ret = mem_read64(mem, core, core->sp + 8, &temp_u64b); // Read operand
		if (ret) return ret;
		ret = mem_write64(mem, core, core->sp + 8, temp_u64 % temp_u64b); // Write remainder
		if (ret) return ret;
		core->sp += 8;
		core->pc += 1;
		break;
	case op_modru8:
		ret = mem_read8(mem, core, core->sp, &temp_u8); // Read operand
		if (ret) return ret;
		ret = mem_read8(mem, core, core->sp + 1, &temp_u8b); // Read operand
		if (ret) return ret;
		ret = mem_write8(mem, core, core->sp + 1, temp_u8b % temp_u8); // Write remainder
		if (ret) return ret;
		core->sp += 1;
		core->pc += 1;
		break;
	case op_modru16:
		ret = mem_read16(mem, core, core->sp, &temp_u16); // Read operand
		if (ret) return ret;
		ret = mem_read16(mem, core, core->sp + 2, &temp_u16b); // Read operand
		if (ret) return ret;
		ret = mem_write16(mem, core, core->sp + 2, temp_u16b % temp_u16); // Write remainder
		if (ret) return ret;
		core->sp += 2;
		core->pc += 1;
		break;
	case op_modru32:
		ret = mem_read32(mem, core, core->sp, &temp_u32); // Read operand
		if (ret) return ret;
		ret = mem_read32(mem, core, core->sp + 4, &temp_u32b); // Read operand
		if (ret) return ret;
		ret = mem_write32(mem, core, core->sp + 4, temp_u32b % temp_u32); // Write remainder
		if (ret) return ret;
		core->sp += 4;
		core->pc += 1;
		break;
	case op_modru64:
		ret = mem_read64(mem, core, core->sp, &temp_u64); // Read operand
		if (ret) return ret;
		ret = mem_read64(mem, core, core->sp + 8, &temp_u64b); // Read operand
		if (ret) return ret;
		ret = mem_write64(mem, core, core->sp + 8, temp_u64b % temp_u64); // Write remainder
		if (ret) return ret;
		core->sp += 8;
		core->pc += 1;
		break;
	case op_modi8:
		ret = mem_read8(mem, core, core->sp, (uint8_t*)&temp_i8); // Read operand
		if (ret) return ret;
		ret = mem_read8(mem, core, core->sp + 1, (uint8_t*)&temp_i8b); // Read operand
		if (ret) return ret;
		ret = mem_write8(mem, core, core->sp + 1, temp_i8 % temp_i8b); // Write remainder
		if (ret) return ret;
		core->sp += 1;
		core->pc += 1;
		break;
	case op_modi16:
		ret = mem_read16(mem, core, core->sp, (uint16_t*)&temp_i16); // Read operand
		if (ret) return ret;
		ret = mem_read16(mem, core, core->sp + 2, (uint16_t*)&temp_i16b); // Read operand
		if (ret) return ret;
		ret = mem_write16(mem, core, core->sp + 2, temp_i16 % temp_i16b); // Write remainder
		if (ret) return ret;
		core->sp += 2;
		core->pc += 1;
		break;
	case op_modi32:
		ret = mem_read32(mem, core, core->sp, (uint32_t*)&temp_i32); // Read operand
		if (ret) return ret;
		ret = mem_read32(mem, core, core->sp + 4, (uint32_t*)&temp_i32b); // Read operand
		if (ret) return ret;
		ret = mem_write32(mem, core, core->sp + 4, temp_i32 % temp_i32b); // Write remainder
		if (ret) return ret;
		core->sp += 4;
		core->pc += 1;
		break;
	case op_modi64:
		ret = mem_read64(mem, core, core->sp, (uint64_t*)&temp_i64); // Read operand
		if (ret) return ret;
		ret = mem_read64(mem, core, core->sp + 8, (uint64_t*)&temp_i64b); // Read operand
		if (ret) return ret;
		ret = mem_write64(mem, core, core->sp + 8, temp_i64 % temp_i64b); // Write remainder
		if (ret) return ret;
		core->sp += 8;
		core->pc += 1;
		break;
	case op_modri8:
		ret = mem_read8(mem, core, core->sp, (uint8_t*)&temp_i8); // Read operand
		if (ret) return ret;
		ret = mem_read8(mem, core, core->sp + 1, (uint8_t*)&temp_i8b); // Read operand
		if (ret) return ret;
		ret = mem_write8(mem, core, core->sp + 1, temp_i8b % temp_i8); // Write remainder
		if (ret) return ret;
		core->sp += 1;
		core->pc += 1;
		break;
	case op_modri16:
		ret = mem_read16(mem, core, core->sp, (uint16_t*)&temp_i16); // Read operand
		if (ret) return ret;
		ret = mem_read16(mem, core, core->sp + 2, (uint16_t*)&temp_i16b); // Read operand
		if (ret) return ret;
		ret = mem_write16(mem, core, core->sp + 2, temp_i16b % temp_i16); // Write remainder
		if (ret) return ret;
		core->sp += 2;
		core->pc += 1;
		break;
	case op_modri32:
		ret = mem_read32(mem, core, core->sp, (uint32_t*)&temp_i32); // Read operand
		if (ret) return ret;
		ret = mem_read32(mem, core, core->sp + 4, (uint32_t*)&temp_i32b); // Read operand
		if (ret) return ret;
		ret = mem_write32(mem, core, core->sp + 4, temp_i32b % temp_i32); // Write remainder
		if (ret) return ret;
		core->sp += 4;
		core->pc += 1;
		break;
	case op_modri64:
		ret = mem_read64(mem, core, core->sp, (uint64_t*)&temp_i64); // Read operand
		if (ret) return ret;
		ret = mem_read64(mem, core, core->sp + 8, (uint64_t*)&temp_i64b); // Read operand
		if (ret) return ret;
		ret = mem_write64(mem, core, core->sp + 8, temp_i64b % temp_i64); // Write remainder
		if (ret) return ret;
		core->sp += 8;
		core->pc += 1;
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

	//
	// Function operations
	//
	case op_call:
	case op_calls:
	case op_ret:

	//
	// Branching operations
	//
	case op_jmp:
	case op_jmps:
	case op_rjmpi8:
	case op_rjmpi16:
	case op_rjmpi32:
	case op_rjmpi64:

	//
	// Conditional branching operations
	//
	case op_brz8:
	case op_brz16:
	case op_brz32:
	case op_brz64:
	case op_rbrz8i64:
	case op_rbrz16i64:
	case op_rbrz32i64:
	case op_rbrz64i64:
		break;

	//
	// Memory operations
	//
	case op_load8:
		ret = mem_read64(mem, core, core->sp, &temp_addr); // Read addr
		if (ret) return ret;
		ret = mem_read8(mem, core, temp_addr, &temp_u8); // Read data
		if (ret) return ret;
		ret = mem_write8(mem, core, core->sp - 1, temp_u8); // Write to stack
		if (ret) return ret;
		core->sp -= 1;
		core->pc += 1;
		break;
	case op_load16:
		ret = mem_read64(mem, core, core->sp, &temp_addr); // Read addr
		if (ret) return ret;
		ret = mem_read16(mem, core, temp_addr, &temp_u16); // Read data
		if (ret) return ret;
		ret = mem_write16(mem, core, core->sp - 2, temp_u16); // Write to stack
		if (ret) return ret;
		core->sp -= 2;
		core->pc += 1;
		break;
	case op_load32:
		ret = mem_read64(mem, core, core->sp, &temp_addr); // Read addr
		if (ret) return ret;
		ret = mem_read32(mem, core, temp_addr, &temp_u32); // Read data
		if (ret) return ret;
		ret = mem_write32(mem, core, core->sp - 4, temp_u32); // Write to stack
		if (ret) return ret;
		core->sp -= 4;
		core->pc += 1;
		break;
	case op_load64:
		ret = mem_read64(mem, core, core->sp, &temp_addr); // Read addr
		if (ret) return ret;
		ret = mem_read64(mem, core, temp_addr, &temp_u64); // Read data
		if (ret) return ret;
		ret = mem_write64(mem, core, core->sp - 8, temp_u64); // Write to stack
		if (ret) return ret;
		core->sp -= 8;
		core->pc += 1;
		break;
	case op_loadpop8:
		ret = mem_read64(mem, core, core->sp, &temp_addr); // Read addr
		if (ret) return ret;
		ret = mem_read8(mem, core, temp_addr, &temp_u8); // Read data
		if (ret) return ret;
		ret = mem_write8(mem, core, core->sp + 7, temp_u8); // Write to stack
		if (ret) return ret;
		core->sp += 7;
		core->pc += 1;
		break;
	case op_loadpop16:
		ret = mem_read64(mem, core, core->sp, &temp_addr); // Read addr
		if (ret) return ret;
		ret = mem_read16(mem, core, temp_addr, &temp_u16); // Read data
		if (ret) return ret;
		ret = mem_write16(mem, core, core->sp + 6, temp_u16); // Write to stack
		if (ret) return ret;
		core->sp += 6;
		core->pc += 1;
		break;
	case op_loadpop32:
		ret = mem_read64(mem, core, core->sp, &temp_addr); // Read addr
		if (ret) return ret;
		ret = mem_read32(mem, core, temp_addr, &temp_u32); // Read data
		if (ret) return ret;
		ret = mem_write32(mem, core, core->sp + 4, temp_u32); // Write to stack
		if (ret) return ret;
		core->sp += 4;
		core->pc += 1;
		break;
	case op_loadpop64:
		ret = mem_read64(mem, core, core->sp, &temp_addr); // Read addr
		if (ret) return ret;
		ret = mem_read64(mem, core, temp_addr, &temp_u64); // Read data
		if (ret) return ret;
		ret = mem_write64(mem, core, core->sp, temp_u64); // Write to stack
		if (ret) return ret;
		core->pc += 1;
		break;
	case op_loadsfp8:
		ret = mem_read64(mem, core, core->sp, (uint64_t*)&temp_i64); // Read offset
		if (ret) return ret;
		ret = mem_read8(mem, core, temp_i64 + core->sfp, &temp_u8); // Read data
		if (ret) return ret;
		ret = mem_write8(mem, core, core->sp - 1, temp_u8); // Write to stack
		if (ret) return ret;
		core->sp -= 1;
		core->pc += 1;
		break;
	case op_loadsfp16:
		ret = mem_read64(mem, core, core->sp, (uint64_t*)&temp_i64); // Read offset
		if (ret) return ret;
		ret = mem_read16(mem, core, temp_i64 + core->sfp, &temp_u16); // Read data
		if (ret) return ret;
		ret = mem_write16(mem, core, core->sp - 2, temp_u16); // Write to stack
		if (ret) return ret;
		core->sp -= 2;
		core->pc += 1;
		break;
	case op_loadsfp32:
		ret = mem_read64(mem, core, core->sp, (uint64_t*)&temp_i64); // Read offset
		if (ret) return ret;
		ret = mem_read32(mem, core, temp_i64 + core->sfp, &temp_u32); // Read data
		if (ret) return ret;
		ret = mem_write32(mem, core, core->sp - 4, temp_u32); // Write to stack
		if (ret) return ret;
		core->sp -= 4;
		core->pc += 1;
		break;
	case op_loadsfp64:
		ret = mem_read64(mem, core, core->sp, (uint64_t*)&temp_i64); // Read offset
		if (ret) return ret;
		ret = mem_read64(mem, core, temp_i64 + core->sfp, &temp_u64); // Read data
		if (ret) return ret;
		ret = mem_write64(mem, core, core->sp - 8, temp_u64); // Write to stack
		if (ret) return ret;
		core->sp -= 8;
		core->pc += 1;
		break;
	case op_loadpopsfp8:
		ret = mem_read64(mem, core, core->sp, (uint64_t*)&temp_i64); // Read offset
		if (ret) return ret;
		ret = mem_read8(mem, core, temp_i64 + core->sfp, &temp_u8); // Read data
		if (ret) return ret;
		ret = mem_write8(mem, core, core->sp + 7, temp_u8); // Write to stack
		if (ret) return ret;
		core->sp += 7;
		core->pc += 1;
		break;
	case op_loadpopsfp16:
		ret = mem_read64(mem, core, core->sp, (uint64_t*)&temp_i64); // Read offset
		if (ret) return ret;
		ret = mem_read16(mem, core, temp_i64 + core->sfp, &temp_u16); // Read data
		if (ret) return ret;
		ret = mem_write16(mem, core, core->sp + 6, temp_u16); // Write to stack
		if (ret) return ret;
		core->sp += 6;
		core->pc += 1;
		break;
	case op_loadpopsfp32:
		ret = mem_read64(mem, core, core->sp, (uint64_t*)&temp_i64); // Read offset
		if (ret) return ret;
		ret = mem_read32(mem, core, temp_i64 + core->sfp, &temp_u32); // Read data
		if (ret) return ret;
		ret = mem_write32(mem, core, core->sp + 4, temp_u32); // Write to stack
		if (ret) return ret;
		core->sp += 4;
		core->pc += 1;
		break;
	case op_loadpopsfp64:
		ret = mem_read64(mem, core, core->sp, (uint64_t*)&temp_i64); // Read offset
		if (ret) return ret;
		ret = mem_read64(mem, core, temp_i64 + core->sfp, &temp_u64); // Read data
		if (ret) return ret;
		ret = mem_write64(mem, core, core->sp, temp_u64); // Write to stack
		if (ret) return ret;
		core->pc += 1;
		break;
	case op_store8:
		ret = mem_read64(mem, core, core->sp, &temp_addr); // Read addr
		if (ret) return ret;
		ret = mem_read8(mem, core, core->sp + 8, &temp_u8); // Read data
		if (ret) return ret;
		ret = mem_write8(mem, core, temp_addr, temp_u8); // Write to memory
		if (ret) return ret;
		core->pc += 1;
		break;
	case op_store16:
		ret = mem_read64(mem, core, core->sp, &temp_addr); // Read addr
		if (ret) return ret;
		ret = mem_read16(mem, core, core->sp + 8, &temp_u16); // Read data
		if (ret) return ret;
		ret = mem_write16(mem, core, temp_addr, temp_u16); // Write to memory
		if (ret) return ret;
		core->pc += 1;
		break;
	case op_store32:
		ret = mem_read64(mem, core, core->sp, &temp_addr); // Read addr
		if (ret) return ret;
		ret = mem_read32(mem, core, core->sp + 8, &temp_u32); // Read data
		if (ret) return ret;
		ret = mem_write32(mem, core, temp_addr, temp_u32); // Write to memory
		if (ret) return ret;
		core->pc += 1;
		break;
	case op_store64:
		ret = mem_read64(mem, core, core->sp, &temp_addr); // Read addr
		if (ret) return ret;
		ret = mem_read64(mem, core, core->sp + 8, &temp_u64); // Read data
		if (ret) return ret;
		ret = mem_write64(mem, core, temp_addr, temp_u64); // Write to memory
		if (ret) return ret;
		core->pc += 1;
		break;
	case op_storepop8:
		ret = mem_read64(mem, core, core->sp, &temp_addr); // Read addr
		if (ret) return ret;
		ret = mem_read8(mem, core, core->sp + 8, &temp_u8); // Read data
		if (ret) return ret;
		ret = mem_write8(mem, core, temp_addr, temp_u8); // Write to memory
		if (ret) return ret;
		core->sp += 8;
		core->pc += 1;
		break;
	case op_storepop16:
		ret = mem_read64(mem, core, core->sp, &temp_addr); // Read addr
		if (ret) return ret;
		ret = mem_read16(mem, core, core->sp + 8, &temp_u16); // Read data
		if (ret) return ret;
		ret = mem_write16(mem, core, temp_addr, temp_u16); // Write to memory
		if (ret) return ret;
		core->sp += 8;
		core->pc += 1;
		break;
	case op_storepop32:
		ret = mem_read64(mem, core, core->sp, &temp_addr); // Read addr
		if (ret) return ret;
		ret = mem_read32(mem, core, core->sp + 8, &temp_u32); // Read data
		if (ret) return ret;
		ret = mem_write32(mem, core, temp_addr, temp_u32); // Write to memory
		if (ret) return ret;
		core->sp += 8;
		core->pc += 1;
		break;
	case op_storepop64:
		ret = mem_read64(mem, core, core->sp, &temp_addr); // Read addr
		if (ret) return ret;
		ret = mem_read64(mem, core, core->sp + 8, &temp_u64); // Read data
		if (ret) return ret;
		ret = mem_write64(mem, core, temp_addr, temp_u64); // Write to memory
		if (ret) return ret;
		core->sp += 8;
		core->pc += 1;
		break;
	case op_storesfp8:
		ret = mem_read64(mem, core, core->sp, (uint64_t*)&temp_i64); // Read offset
		if (ret) return ret;
		ret = mem_read8(mem, core, core->sp + 8, &temp_u8); // Read data
		if (ret) return ret;
		ret = mem_write8(mem, core, core->sfp + temp_i64, temp_u8); // Write to memory
		if (ret) return ret;
		core->pc += 1;
		break;
	case op_storesfp16:
		ret = mem_read64(mem, core, core->sp, (uint64_t*)&temp_i64); // Read offset
		if (ret) return ret;
		ret = mem_read16(mem, core, core->sp + 8, &temp_u16); // Read data
		if (ret) return ret;
		ret = mem_write16(mem, core, core->sfp + temp_i64, temp_u16); // Write to memory
		if (ret) return ret;
		core->pc += 1;
		break;
	case op_storesfp32:
		ret = mem_read64(mem, core, core->sp, (uint64_t*)&temp_i64); // Read offset
		if (ret) return ret;
		ret = mem_read32(mem, core, core->sp + 8, &temp_u32); // Read data
		if (ret) return ret;
		ret = mem_write32(mem, core, core->sfp + temp_i64, temp_u32); // Write to memory
		if (ret) return ret;
		core->pc += 1;
		break;
	case op_storesfp64:
		ret = mem_read64(mem, core, core->sp, (uint64_t*)&temp_i64); // Read offset
		if (ret) return ret;
		ret = mem_read64(mem, core, core->sp + 8, &temp_u64); // Read data
		if (ret) return ret;
		ret = mem_write64(mem, core, core->sfp + temp_i64, temp_u64); // Write to memory
		if (ret) return ret;
		core->pc += 1;
		break;
	case op_storepopsfp8:
		ret = mem_read64(mem, core, core->sp, (uint64_t*)&temp_i64); // Read offset
		if (ret) return ret;
		ret = mem_read8(mem, core, core->sp + 8, &temp_u8); // Read data
		if (ret) return ret;
		ret = mem_write8(mem, core, core->sfp + temp_i64, temp_u8); // Write to memory
		if (ret) return ret;
		core->sp += 8;
		core->pc += 1;
		break;
	case op_storepopsfp16:
		ret = mem_read64(mem, core, core->sp, (uint64_t*)&temp_i64); // Read offset
		if (ret) return ret;
		ret = mem_read16(mem, core, core->sp + 8, &temp_u16); // Read data
		if (ret) return ret;
		ret = mem_write16(mem, core, core->sfp + temp_i64, temp_u16); // Write to memory
		if (ret) return ret;
		core->sp += 8;
		core->pc += 1;
		break;
	case op_storepopsfp32:
		ret = mem_read64(mem, core, core->sp, (uint64_t*)&temp_i64); // Read offset
		if (ret) return ret;
		ret = mem_read32(mem, core, core->sp + 8, &temp_u32); // Read data
		if (ret) return ret;
		ret = mem_write32(mem, core, core->sfp + temp_i64, temp_u32); // Write to memory
		if (ret) return ret;
		core->sp += 8;
		core->pc += 1;
		break;
	case op_storepopsfp64:
		ret = mem_read64(mem, core, core->sp, (uint64_t*)&temp_i64); // Read offset
		if (ret) return ret;
		ret = mem_read64(mem, core, core->sp + 8, &temp_u64); // Read data
		if (ret) return ret;
		ret = mem_write64(mem, core, core->sfp + temp_i64, temp_u64); // Write to memory
		if (ret) return ret;
		core->sp += 8;
		core->pc += 1;
		break;
	case op_storer8:
		ret = mem_read64(mem, core, core->sp + 1, &temp_addr); // Read addr
		if (ret) return ret;
		ret = mem_read8(mem, core, core->sp, &temp_u8); // Read data
		if (ret) return ret;
		ret = mem_write8(mem, core, temp_addr, temp_u8); // Write to memory
		if (ret) return ret;
		core->pc += 1;
		break;
	case op_storer16:
		ret = mem_read64(mem, core, core->sp + 2, &temp_addr); // Read addr
		if (ret) return ret;
		ret = mem_read16(mem, core, core->sp, &temp_u16); // Read data
		if (ret) return ret;
		ret = mem_write16(mem, core, temp_addr, temp_u16); // Write to memory
		if (ret) return ret;
		core->pc += 1;
		break;
	case op_storer32:
		ret = mem_read64(mem, core, core->sp + 4, &temp_addr); // Read addr
		if (ret) return ret;
		ret = mem_read32(mem, core, core->sp, &temp_u32); // Read data
		if (ret) return ret;
		ret = mem_write32(mem, core, temp_addr, temp_u32); // Write to memory
		if (ret) return ret;
		core->pc += 1;
		break;
	case op_storer64:
		ret = mem_read64(mem, core, core->sp + 8, &temp_addr); // Read addr
		if (ret) return ret;
		ret = mem_read64(mem, core, core->sp, &temp_u64); // Read data
		if (ret) return ret;
		ret = mem_write64(mem, core, temp_addr, temp_u64); // Write to memory
		if (ret) return ret;
		core->pc += 1;
		break;
	case op_storerpop8:
		ret = mem_read64(mem, core, core->sp + 1, &temp_addr); // Read addr
		if (ret) return ret;
		ret = mem_read8(mem, core, core->sp, &temp_u8); // Read data
		if (ret) return ret;
		ret = mem_write8(mem, core, temp_addr, temp_u8); // Write to memory
		if (ret) return ret;
		core->sp += 1;
		core->pc += 1;
		break;
	case op_storerpop16:
		ret = mem_read64(mem, core, core->sp + 2, &temp_addr); // Read addr
		if (ret) return ret;
		ret = mem_read16(mem, core, core->sp, &temp_u16); // Read data
		if (ret) return ret;
		ret = mem_write16(mem, core, temp_addr, temp_u16); // Write to memory
		if (ret) return ret;
		core->sp += 2;
		core->pc += 1;
		break;
	case op_storerpop32:
		ret = mem_read64(mem, core, core->sp + 4, &temp_addr); // Read addr
		if (ret) return ret;
		ret = mem_read32(mem, core, core->sp, &temp_u32); // Read data
		if (ret) return ret;
		ret = mem_write32(mem, core, temp_addr, temp_u32); // Write to memory
		if (ret) return ret;
		core->sp += 4;
		core->pc += 1;
		break;
	case op_storerpop64:
		ret = mem_read64(mem, core, core->sp + 8, &temp_addr); // Read addr
		if (ret) return ret;
		ret = mem_read64(mem, core, core->sp, &temp_u64); // Read data
		if (ret) return ret;
		ret = mem_write64(mem, core, temp_addr, temp_u64); // Write to memory
		if (ret) return ret;
		core->sp += 8;
		core->pc += 1;
		break;
	case op_storersfp8:
		ret = mem_read64(mem, core, core->sp + 1, (uint64_t*)&temp_i64); // Read offset
		if (ret) return ret;
		ret = mem_read8(mem, core, core->sp, &temp_u8); // Read data
		if (ret) return ret;
		ret = mem_write8(mem, core, core->sfp + temp_i64, temp_u8); // Write to memory
		if (ret) return ret;
		core->pc += 1;
		break;
	case op_storersfp16:
		ret = mem_read64(mem, core, core->sp + 2, (uint64_t*)&temp_i64); // Read offset
		if (ret) return ret;
		ret = mem_read16(mem, core, core->sp, &temp_u16); // Read data
		if (ret) return ret;
		ret = mem_write16(mem, core, core->sfp + temp_i64, temp_u16); // Write to memory
		if (ret) return ret;
		core->pc += 1;
		break;
	case op_storersfp32:
		ret = mem_read64(mem, core, core->sp + 4, (uint64_t*)&temp_i64); // Read offset
		if (ret) return ret;
		ret = mem_read32(mem, core, core->sp, &temp_u32); // Read data
		if (ret) return ret;
		ret = mem_write32(mem, core, core->sfp + temp_i64, temp_u32); // Write to memory
		if (ret) return ret;
		core->pc += 1;
		break;
	case op_storersfp64:
		ret = mem_read64(mem, core, core->sp + 8, (uint64_t*)&temp_i64); // Read offset
		if (ret) return ret;
		ret = mem_read64(mem, core, core->sp, &temp_u64); // Read data
		if (ret) return ret;
		ret = mem_write64(mem, core, core->sfp + temp_i64, temp_u64); // Write to memory
		if (ret) return ret;
		core->pc += 1;
		break;
	case op_storerpopsfp8:
		ret = mem_read64(mem, core, core->sp + 1, (uint64_t*)&temp_i64); // Read offset
		if (ret) return ret;
		ret = mem_read8(mem, core, core->sp, &temp_u8); // Read data
		if (ret) return ret;
		ret = mem_write8(mem, core, core->sfp + temp_i64, temp_u8); // Write to memory
		if (ret) return ret;
		core->sp += 1;
		core->pc += 1;
		break;
	case op_storerpopsfp16:
		ret = mem_read64(mem, core, core->sp + 2, (uint64_t*)&temp_i64); // Read offset
		if (ret) return ret;
		ret = mem_read16(mem, core, core->sp, &temp_u16); // Read data
		if (ret) return ret;
		ret = mem_write16(mem, core, core->sfp + temp_i64, temp_u16); // Write to memory
		if (ret) return ret;
		core->sp += 2;
		core->pc += 1;
		break;
	case op_storerpopsfp32:
		ret = mem_read64(mem, core, core->sp + 4, (uint64_t*)&temp_i64); // Read offset
		if (ret) return ret;
		ret = mem_read32(mem, core, core->sp, &temp_u32); // Read data
		if (ret) return ret;
		ret = mem_write32(mem, core, core->sfp + temp_i64, temp_u32); // Write to memory
		if (ret) return ret;
		core->sp += 4;
		core->pc += 1;
		break;
	case op_storerpopsfp64:
		ret = mem_read64(mem, core, core->sp + 8, (uint64_t*)&temp_i64); // Read offset
		if (ret) return ret;
		ret = mem_read64(mem, core, core->sp, &temp_u64); // Read data
		if (ret) return ret;
		ret = mem_write64(mem, core, core->sfp + temp_i64, temp_u64); // Write to memory
		if (ret) return ret;
		core->sp += 8;
		core->pc += 1;
		break;

	//
	// Special Operations
	//
	case op_setsbp:
		ret = mem_read64(mem, core, core->sp, &temp_addr); // Read addr
		if (ret) return ret;
		core->sbp = temp_addr;
		core->sp += 8;
		core->pc += 1;
		break;
	case op_setsfp:
		ret = mem_read64(mem, core, core->sp, &temp_addr); // Read addr
		if (ret) return ret;
		core->sfp = temp_addr;
		core->sp += 8;
		core->pc += 1;
		break;
	case op_setsp:
		ret = mem_read64(mem, core, core->sp, &temp_addr); // Read addr
		if (ret) return ret;
		core->sp = temp_addr;
		core->pc += 1;
		break;
	case op_halt:
		return STERR_HALT;
	case op_ext:
	case op_nop:
		return -1;

	default:
		return -1;
	}

	return 0;
}
