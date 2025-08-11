// core.c

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/random.h>
#include <unistd.h>

#include "core.h"
#include "starch.h"

// Constants
enum {
	STDINOUT_BUFF_SIZE = 0x400,
};

void core_init(struct core *core)
{
	memset(core, 0, sizeof(struct core));
	core->pc = INIT_PC_VAL;
	core->stdin_buff = (uint8_t*)malloc(STDINOUT_BUFF_SIZE);
	core->stdout_buff = (uint8_t*)malloc(STDINOUT_BUFF_SIZE);
}

void core_destroy(struct core *core)
{
	free(core->stdout_buff);
	core->stdout_buff = NULL;
	free(core->stdin_buff);
	core->stdin_buff = NULL;
}

// @todo: Could move to utility library
// Read buflen random bytes into the buffer at buf.
// Returns zero on success, negative on failure.
static int core_get_random(void *buf, int buflen)
{
	int count;
	for (count = 0; count < buflen; ) {
		int ret = getrandom((uint8_t*)buf + count, buflen - count, 0);
		if (ret < 0) {
			return ret;
		}
		count += ret;
	}
	return 0;
}

static int core_read_stdin(struct core *core, uint8_t *b)
{
	int ret = 0;
	if (core->stdin_head != core->stdin_tail) {
		// There is already buffered data available
		*b = core->stdin_buff[core->stdin_head++];
	}
	else {
		// Read available up to buffer size
		core->stdin_head = 0;
		ssize_t bc = read(0, core->stdin_buff, STDINOUT_BUFF_SIZE);
		if (bc > 0) {
			core->stdin_tail = bc;
		}
		else {
			core->stdin_tail = 0;
			ret = errno ? errno : 1;
		}
	}
	return ret;
}

static int core_flush_stdout(struct core *core)
{
	// Flush to stdout
	int ret = 0;
	ssize_t bc = write(1, core->stdout_buff, core->stdout_count);
	if (bc != core->stdout_count) ret = errno;
	core->stdout_count = 0;
	return ret;
}

static int core_write_stdout(struct core *core, uint8_t b)
{
	int ret = 0;
	core->stdout_buff[core->stdout_count++] = b;
	if (core->stdout_count >= STDINOUT_BUFF_SIZE || b == '\n') {
		// Flush when buffer fills or newline is written
		ret = core_flush_stdout(core);
	}
	return ret;
}

static int core_mem_write8(struct core *core, struct mem *mem, uint64_t addr, uint8_t data)
{
	// Check frame access
	if (addr < core->sbp && addr >= core->sfp && addr < core->sfp + 16) {
		return STINT_BAD_FRAME_ACCESS;
	}

	// Check IO memory
	if (addr < END_IO_ADDR) {
		if (addr == IO_STDOUT_ADDR) {
			return core_write_stdout(core, data);
		}
		if (addr == IO_FLUSH_ADDR) {
			return core_flush_stdout(core);
		}
		if (addr == IO_ASSERT_ADDR) {
			return data == 0 ? STINT_ASSERT_FAILURE : 0;
		}
		return STINT_BAD_IO_ACCESS;
	}

	return mem_write8(mem, addr, data);
}

static int core_stack_write8(struct core *core, struct mem *mem, uint64_t addr, uint8_t data)
{
	// Check stack bounds
	if (addr >= core->sbp || addr < core->slp || addr < END_IO_ADDR) {
		return STINT_BAD_STACK_ACCESS;
	}

	return core_mem_write8(core, mem, addr, data);
}

static int core_mem_write16(struct core *core, struct mem *mem, uint64_t addr, uint16_t data)
{
	// Check frame access
	if (addr < core->sbp && addr >= core->sfp - 1 && addr < core->sfp + 16) {
		return STINT_BAD_FRAME_ACCESS;
	}

	// Check IO memory
	if (addr < END_IO_ADDR) {
		if (addr == IO_ASSERT_ADDR) {
			return data == 0 ? STINT_ASSERT_FAILURE : 0;
		}
		return STINT_BAD_IO_ACCESS; // No 16-bit IO write operations currently
	}

	return mem_write16(mem, addr, data);
}

static int core_stack_write16(struct core *core, struct mem *mem, uint64_t addr, uint16_t data)
{
	// Check stack bounds
	if (addr >= core->sbp - 1 || addr < core->slp || addr < END_IO_ADDR) {
		return STINT_BAD_STACK_ACCESS;
	}

	return core_mem_write16(core, mem, addr, data);
}

static int core_mem_write32(struct core *core, struct mem *mem, uint64_t addr, uint32_t data)
{
	// Check frame access
	if (addr < core->sbp && addr >= core->sfp - 3 && addr < core->sfp + 16) {
		return STINT_BAD_FRAME_ACCESS;
	}

	// Check IO memory
	if (addr < END_IO_ADDR) {
		if (addr == IO_ASSERT_ADDR) {
			return data == 0 ? STINT_ASSERT_FAILURE : 0;
		}
		return STINT_BAD_IO_ACCESS; // No 32-bit IO write operations currently
	}

	return mem_write32(mem, addr, data);
}

static int core_stack_write32(struct core *core, struct mem *mem, uint64_t addr, uint32_t data)
{
	// Check stack bounds
	if (addr >= core->sbp - 3 || addr < core->slp || addr < END_IO_ADDR) {
		return STINT_BAD_STACK_ACCESS;
	}

	return core_mem_write32(core, mem, addr, data);
}

static int core_mem_write64(struct core *core, struct mem *mem, uint64_t addr, uint64_t data)
{
	// Check frame access
	if (addr < core->sbp && addr >= core->sfp - 7 && addr < core->sfp + 16) {
		return STINT_BAD_FRAME_ACCESS;
	}

	// Check IO memory
	if (addr < END_IO_ADDR) {
		if (addr == IO_ASSERT_ADDR) {
			return data == 0 ? STINT_ASSERT_FAILURE : 0;
		}
		return STINT_BAD_IO_ACCESS; // No 64-bit IO write operations currently
	}

	return mem_write64(mem, addr, data);
}

static int core_stack_write64(struct core *core, struct mem *mem, uint64_t addr, uint64_t data)
{
	// Check stack bounds
	if (addr >= core->sbp - 7 || addr < core->slp || addr < END_IO_ADDR) {
		return STINT_BAD_STACK_ACCESS;
	}

	return core_mem_write64(core, mem, addr, data);
}

static int core_mem_read8(struct core *core, struct mem *mem, uint64_t addr, uint8_t *data)
{
	// Check frame access
	if (addr < core->sbp && addr >= core->sfp && addr < core->sfp + 16) {
		return STINT_BAD_FRAME_ACCESS;
	}

	// Check IO memory
	if (addr < END_IO_ADDR) {
		if (addr == IO_STDIN_ADDR) {
			return core_read_stdin(core, data);
		}
		if (addr == IO_URAND_ADDR) {
			return core_get_random(data, 1);
		}
		return STINT_BAD_IO_ACCESS; // No 8-bit IO read operations currently
	}

	return mem_read8(mem, addr, data);
}

static int core_stack_read8(struct core *core, struct mem *mem, uint64_t addr, uint8_t *data)
{
	// Check stack bounds
	if (addr >= core->sbp || addr < core->slp || addr < END_IO_ADDR) {
		return STINT_BAD_STACK_ACCESS;
	}

	return core_mem_read8(core, mem, addr, data);
}

static int core_mem_read16(struct core *core, struct mem *mem, uint64_t addr, uint16_t *data)
{
	// Check frame access
	if (addr < core->sbp && addr >= core->sfp - 1 && addr < core->sfp + 16) {
		return STINT_BAD_FRAME_ACCESS;
	}

	// Check IO memory
	if (addr < END_IO_ADDR) {
		if (addr == IO_URAND_ADDR) {
			return core_get_random(data, 2);
		}
		return STINT_BAD_IO_ACCESS; // No 16-bit IO read operations currently
	}

	return mem_read16(mem, addr, data);
}

static int core_stack_read16(struct core *core, struct mem *mem, uint64_t addr, uint16_t *data)
{
	// Check stack bounds
	if (addr >= core->sbp - 1 || addr < core->slp || addr < END_IO_ADDR) {
		return STINT_BAD_STACK_ACCESS;
	}

	return core_mem_read16(core, mem, addr, data);
}

static int core_mem_read32(struct core *core, struct mem *mem, uint64_t addr, uint32_t *data)
{
	// Check frame access
	if (addr < core->sbp && addr >= core->sfp - 3 && addr < core->sfp + 16) {
		return STINT_BAD_FRAME_ACCESS;
	}

	// Check IO memory
	if (addr < END_IO_ADDR) {
		if (addr == IO_URAND_ADDR) {
			return core_get_random(data, 4);
		}
		return STINT_BAD_IO_ACCESS;
	}

	return mem_read32(mem, addr, data);
}

static int core_stack_read32(struct core *core, struct mem *mem, uint64_t addr, uint32_t *data)
{
	// Check stack bounds
	if (addr >= core->sbp - 3 || addr < core->slp || addr < END_IO_ADDR) {
		return STINT_BAD_STACK_ACCESS;
	}

	return core_mem_read32(core, mem, addr, data);
}

static int core_mem_read64(struct core *core, struct mem *mem, uint64_t addr, uint64_t *data)
{
	// Check frame access
	if (addr < core->sbp && addr >= core->sfp - 7 && addr < core->sfp + 16) {
		return STINT_BAD_FRAME_ACCESS;
	}

	// Check IO memory
	if (addr < END_IO_ADDR) {
		if (addr == IO_URAND_ADDR) {
			return core_get_random(data, 8);
		}
		return STINT_BAD_IO_ACCESS; // No 64-bit IO read operations currently
	}

	return mem_read64(mem, addr, data);
}

static int core_stack_read64(struct core *core, struct mem *mem, uint64_t addr, uint64_t *data)
{
	// Check stack bounds
	if (addr >= core->sbp - 7 || addr < core->slp || addr < END_IO_ADDR) {
		return STINT_BAD_STACK_ACCESS;
	}

	return core_mem_read64(core, mem, addr, data);
}

int core_step(struct core *core, struct mem *mem)
{
	// Fetch instruction from memory
	uint8_t opcode;
	int ret = core_mem_read8(core, mem, core->pc, &opcode);

	// Temporary variables for use by instructions
	uint8_t temp_u8, temp_u8b;
	uint16_t temp_u16, temp_u16b;
	uint32_t temp_u32, temp_u32b;
	uint64_t temp_addr, temp_u64, temp_u64b;

	//
	// Execute instruction on core and memory
	//
	if (ret == 0) switch (opcode) {
	//
	// Invalid instruction
	//
	case op_invalid:
		ret = STINT_INVALID_INST;
		break;

	//
	// Push immediate operations
	//
	case op_push8as8:
		ret = core_mem_read8(core, mem, core->pc + 1, &temp_u8); // Read imm
		if (ret) break;
		ret = core_stack_write8(core, mem, core->sp - 1, temp_u8); // Write to stack
		if (ret) break;
		core->sp -= 1;
		core->pc += 2;
		break;
	case op_push8asu16:
		ret = core_mem_read8(core, mem, core->pc + 1, &temp_u8); // Read imm
		if (ret) break;
		ret = core_stack_write16(core, mem, core->sp - 2, temp_u8); // Write to stack
		if (ret) break;
		core->sp -= 2;
		core->pc += 2;
		break;
	case op_push8asu32:
		ret = core_mem_read8(core, mem, core->pc + 1, &temp_u8); // Read imm
		if (ret) break;
		ret = core_stack_write32(core, mem, core->sp - 4, temp_u8); // Write to stack
		if (ret) break;
		core->sp -= 4;
		core->pc += 2;
		break;
	case op_push8asu64:
		ret = core_mem_read8(core, mem, core->pc + 1, &temp_u8); // Read imm
		if (ret) break;
		ret = core_stack_write64(core, mem, core->sp - 8, temp_u8); // Write to stack
		if (ret) break;
		core->sp -= 8;
		core->pc += 2;
		break;
	case op_push8asi16:
		ret = core_mem_read8(core, mem, core->pc + 1, &temp_u8); // Read imm
		if (ret) break;
		ret = core_stack_write16(core, mem, core->sp - 2, (int8_t)temp_u8); // Write to stack
		if (ret) break;
		core->sp -= 2;
		core->pc += 2;
		break;
	case op_push8asi32:
		ret = core_mem_read8(core, mem, core->pc + 1, &temp_u8); // Read imm
		if (ret) break;
		ret = core_stack_write32(core, mem, core->sp - 4, (int8_t)temp_u8); // Write to stack
		if (ret) break;
		core->sp -= 4;
		core->pc += 2;
		break;
	case op_push8asi64:
		ret = core_mem_read8(core, mem, core->pc + 1, &temp_u8); // Read imm
		if (ret) break;
		ret = core_stack_write64(core, mem, core->sp - 8, (int8_t)temp_u8); // Write to stack
		if (ret) break;
		core->sp -= 8;
		core->pc += 2;
		break;
	case op_push16as16:
		ret = core_mem_read16(core, mem, core->pc + 1, &temp_u16); // Read imm
		if (ret) break;
		ret = core_stack_write16(core, mem, core->sp - 2, temp_u16); // Write to stack
		if (ret) break;
		core->sp -= 2;
		core->pc += 3;
		break;
	case op_push16asu32:
		ret = core_mem_read16(core, mem, core->pc + 1, &temp_u16); // Read imm
		if (ret) break;
		ret = core_stack_write32(core, mem, core->sp - 4, temp_u16); // Write to stack
		if (ret) break;
		core->sp -= 4;
		core->pc += 3;
		break;
	case op_push16asu64:
		ret = core_mem_read16(core, mem, core->pc + 1, &temp_u16); // Read imm
		if (ret) break;
		ret = core_stack_write64(core, mem, core->sp - 8, temp_u16); // Write to stack
		if (ret) break;
		core->sp -= 8;
		core->pc += 3;
		break;
	case op_push16asi32:
		ret = core_mem_read16(core, mem, core->pc + 1, &temp_u16); // Read imm
		if (ret) break;
		ret = core_stack_write32(core, mem, core->sp - 4, (int16_t)temp_u16); // Write to stack
		if (ret) break;
		core->sp -= 4;
		core->pc += 3;
		break;
	case op_push16asi64:
		ret = core_mem_read16(core, mem, core->pc + 1, &temp_u16); // Read imm
		if (ret) break;
		ret = core_stack_write64(core, mem, core->sp - 8, (int16_t)temp_u16); // Write to stack
		if (ret) break;
		core->sp -= 8;
		core->pc += 3;
		break;
	case op_push32as32:
		ret = core_mem_read32(core, mem, core->pc + 1, &temp_u32); // Read imm
		if (ret) break;
		ret = core_stack_write32(core, mem, core->sp - 4, temp_u32); // Write to stack
		if (ret) break;
		core->sp -= 4;
		core->pc += 5;
		break;
	case op_push32asu64:
		ret = core_mem_read32(core, mem, core->pc + 1, &temp_u32); // Read imm
		if (ret) break;
		ret = core_stack_write64(core, mem, core->sp - 8, temp_u32); // Write to stack
		if (ret) break;
		core->sp -= 8;
		core->pc += 5;
		break;
	case op_push32asi64:
		ret = core_mem_read32(core, mem, core->pc + 1, &temp_u32); // Read imm
		if (ret) break;
		ret = core_stack_write64(core, mem, core->sp - 8, (int32_t)temp_u32); // Write to stack
		if (ret) break;
		core->sp -= 8;
		core->pc += 5;
		break;
	case op_push64as64:
		ret = core_mem_read64(core, mem, core->pc + 1, &temp_u64); // Read imm
		if (ret) break;
		ret = core_stack_write64(core, mem, core->sp - 8, temp_u64); // Write to stack
		if (ret) break;
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
		ret = core_stack_read8(core, mem, core->sp, &temp_u8);
		if (ret) break;
		ret = core_stack_write8(core, mem, core->sp - 1, temp_u8);
		if (ret) break;
		core->sp -= 1;
		core->pc += 1;
		break;
	case op_dup16:
		ret = core_stack_read16(core, mem, core->sp, &temp_u16);
		if (ret) break;
		ret = core_stack_write16(core, mem, core->sp - 2, temp_u16);
		if (ret) break;
		core->sp -= 2;
		core->pc += 1;
		break;
	case op_dup32:
		ret = core_stack_read32(core, mem, core->sp, &temp_u32);
		if (ret) break;
		ret = core_stack_write32(core, mem, core->sp - 4, temp_u32);
		if (ret) break;
		core->sp -= 4;
		core->pc += 1;
		break;
	case op_dup64:
		ret = core_stack_read64(core, mem, core->sp, &temp_u64);
		if (ret) break;
		ret = core_stack_write64(core, mem, core->sp - 8, temp_u64);
		if (ret) break;
		core->sp -= 8;
		core->pc += 1;
		break;

	//
	// Setting operations
	//
	case op_set8:
		ret = core_stack_read8(core, mem, core->sp, &temp_u8);
		if (ret) break;
		ret = core_stack_write8(core, mem, core->sp + 1, temp_u8);
		if (ret) break;
		core->sp += 1;
		core->pc += 1;
		break;
	case op_set16:
		ret = core_stack_read16(core, mem, core->sp, &temp_u16);
		if (ret) break;
		ret = core_stack_write16(core, mem, core->sp + 2, temp_u16);
		if (ret) break;
		core->sp += 2;
		core->pc += 1;
		break;
	case op_set32:
		ret = core_stack_read32(core, mem, core->sp, &temp_u32);
		if (ret) break;
		ret = core_stack_write32(core, mem, core->sp + 4, temp_u32);
		if (ret) break;
		core->sp += 4;
		core->pc += 1;
		break;
	case op_set64:
		ret = core_stack_read64(core, mem, core->sp, &temp_u64);
		if (ret) break;
		ret = core_stack_write64(core, mem, core->sp + 8, temp_u64);
		if (ret) break;
		core->sp += 8;
		core->pc += 1;
		break;

	//
	// Promotion operations
	//
	case op_prom8u16:
		ret = core_stack_read8(core, mem, core->sp, &temp_u8);
		if (ret) break;
		ret = core_stack_write16(core, mem, core->sp - 1, temp_u8);
		if (ret) break;
		core->sp -= 1;
		core->pc += 1;
		break;
	case op_prom8u32:
		ret = core_stack_read8(core, mem, core->sp, &temp_u8);
		if (ret) break;
		ret = core_stack_write32(core, mem, core->sp - 3, temp_u8);
		if (ret) break;
		core->sp -= 3;
		core->pc += 1;
		break;
	case op_prom8u64:
		ret = core_stack_read8(core, mem, core->sp, &temp_u8);
		if (ret) break;
		ret = core_stack_write64(core, mem, core->sp - 7, temp_u8);
		if (ret) break;
		core->sp -= 7;
		core->pc += 1;
		break;
	case op_prom8i16:
		ret = core_stack_read8(core, mem, core->sp, &temp_u8);
		if (ret) break;
		ret = core_stack_write16(core, mem, core->sp - 1, (int8_t)temp_u8);
		if (ret) break;
		core->sp -= 1;
		core->pc += 1;
		break;
	case op_prom8i32:
		ret = core_stack_read8(core, mem, core->sp, &temp_u8);
		if (ret) break;
		ret = core_stack_write32(core, mem, core->sp - 3, (int8_t)temp_u8);
		if (ret) break;
		core->sp -= 3;
		core->pc += 1;
		break;
	case op_prom8i64:
		ret = core_stack_read8(core, mem, core->sp, &temp_u8);
		if (ret) break;
		ret = core_stack_write64(core, mem, core->sp - 7, (int8_t)temp_u8);
		if (ret) break;
		core->sp -= 7;
		core->pc += 1;
		break;
	case op_prom16u32:
		ret = core_stack_read16(core, mem, core->sp, &temp_u16);
		if (ret) break;
		ret = core_stack_write32(core, mem, core->sp - 2, temp_u16);
		if (ret) break;
		core->sp -= 2;
		core->pc += 1;
		break;
	case op_prom16u64:
		ret = core_stack_read16(core, mem, core->sp, &temp_u16);
		if (ret) break;
		ret = core_stack_write64(core, mem, core->sp - 6, temp_u16);
		if (ret) break;
		core->sp -= 6;
		core->pc += 1;
		break;
	case op_prom16i32:
		ret = core_stack_read16(core, mem, core->sp, &temp_u16);
		if (ret) break;
		ret = core_stack_write32(core, mem, core->sp - 2, (int16_t)temp_u16);
		if (ret) break;
		core->sp -= 2;
		core->pc += 1;
		break;
	case op_prom16i64:
		ret = core_stack_read16(core, mem, core->sp, &temp_u16);
		if (ret) break;
		ret = core_stack_write64(core, mem, core->sp - 6, (int16_t)temp_u16);
		if (ret) break;
		core->sp -= 6;
		core->pc += 1;
		break;
	case op_prom32u64:
		ret = core_stack_read32(core, mem, core->sp, &temp_u32);
		if (ret) break;
		ret = core_stack_write64(core, mem, core->sp - 4, temp_u32);
		if (ret) break;
		core->sp -= 4;
		core->pc += 1;
		break;
	case op_prom32i64:
		ret = core_stack_read32(core, mem, core->sp, &temp_u32);
		if (ret) break;
		ret = core_stack_write64(core, mem, core->sp - 4, (int32_t)temp_u32);
		if (ret) break;
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
		ret = core_stack_read8(core, mem, core->sp, &temp_u8b); // Read operand
		if (ret) break;
		ret = core_stack_read8(core, mem, core->sp + 1, &temp_u8); // Read operand
		if (ret) break;
		ret = core_stack_write8(core, mem, core->sp + 1, temp_u8 + temp_u8b); // Write sum
		if (ret) break;
		core->sp += 1;
		core->pc += 1;
		break;
	case op_add16:
		ret = core_stack_read16(core, mem, core->sp, &temp_u16b); // Read operand
		if (ret) break;
		ret = core_stack_read16(core, mem, core->sp + 2, &temp_u16); // Read operand
		if (ret) break;
		ret = core_stack_write16(core, mem, core->sp + 2, temp_u16 + temp_u16b); // Write sum
		if (ret) break;
		core->sp += 2;
		core->pc += 1;
		break;
	case op_add32:
		ret = core_stack_read32(core, mem, core->sp, &temp_u32b); // Read operand
		if (ret) break;
		ret = core_stack_read32(core, mem, core->sp + 4, &temp_u32); // Read operand
		if (ret) break;
		ret = core_stack_write32(core, mem, core->sp + 4, temp_u32 + temp_u32b); // Write sum
		if (ret) break;
		core->sp += 4;
		core->pc += 1;
		break;
	case op_add64:
		ret = core_stack_read64(core, mem, core->sp, &temp_u64b); // Read operand
		if (ret) break;
		ret = core_stack_read64(core, mem, core->sp + 8, &temp_u64); // Read operand
		if (ret) break;
		ret = core_stack_write64(core, mem, core->sp + 8, temp_u64 + temp_u64b); // Write sum
		if (ret) break;
		core->sp += 8;
		core->pc += 1;
		break;
	case op_sub8:
		ret = core_stack_read8(core, mem, core->sp, &temp_u8b); // Read operand
		if (ret) break;
		ret = core_stack_read8(core, mem, core->sp + 1, &temp_u8); // Read operand
		if (ret) break;
		ret = core_stack_write8(core, mem, core->sp + 1, temp_u8 - temp_u8b); // Write difference
		if (ret) break;
		core->sp += 1;
		core->pc += 1;
		break;
	case op_sub16:
		ret = core_stack_read16(core, mem, core->sp, &temp_u16b); // Read operand
		if (ret) break;
		ret = core_stack_read16(core, mem, core->sp + 2, &temp_u16); // Read operand
		if (ret) break;
		ret = core_stack_write16(core, mem, core->sp + 2, temp_u16 - temp_u16b); // Write difference
		if (ret) break;
		core->sp += 2;
		core->pc += 1;
		break;
	case op_sub32:
		ret = core_stack_read32(core, mem, core->sp, &temp_u32b); // Read operand
		if (ret) break;
		ret = core_stack_read32(core, mem, core->sp + 4, &temp_u32); // Read operand
		if (ret) break;
		ret = core_stack_write32(core, mem, core->sp + 4, temp_u32 - temp_u32b); // Write difference
		if (ret) break;
		core->sp += 4;
		core->pc += 1;
		break;
	case op_sub64:
		ret = core_stack_read64(core, mem, core->sp, &temp_u64b); // Read operand
		if (ret) break;
		ret = core_stack_read64(core, mem, core->sp + 8, &temp_u64); // Read operand
		if (ret) break;
		ret = core_stack_write64(core, mem, core->sp + 8, temp_u64 - temp_u64b); // Write difference
		if (ret) break;
		core->sp += 8;
		core->pc += 1;
		break;
	case op_subr8:
		ret = core_stack_read8(core, mem, core->sp, &temp_u8b); // Read operand
		if (ret) break;
		ret = core_stack_read8(core, mem, core->sp + 1, &temp_u8); // Read operand
		if (ret) break;
		ret = core_stack_write8(core, mem, core->sp + 1, temp_u8b - temp_u8); // Write difference
		if (ret) break;
		core->sp += 1;
		core->pc += 1;
		break;
	case op_subr16:
		ret = core_stack_read16(core, mem, core->sp, &temp_u16b); // Read operand
		if (ret) break;
		ret = core_stack_read16(core, mem, core->sp + 2, &temp_u16); // Read operand
		if (ret) break;
		ret = core_stack_write16(core, mem, core->sp + 2, temp_u16b - temp_u16); // Write difference
		if (ret) break;
		core->sp += 2;
		core->pc += 1;
		break;
	case op_subr32:
		ret = core_stack_read32(core, mem, core->sp, &temp_u32b); // Read operand
		if (ret) break;
		ret = core_stack_read32(core, mem, core->sp + 4, &temp_u32); // Read operand
		if (ret) break;
		ret = core_stack_write32(core, mem, core->sp + 4, temp_u32b - temp_u32); // Write difference
		if (ret) break;
		core->sp += 4;
		core->pc += 1;
		break;
	case op_subr64:
		ret = core_stack_read64(core, mem, core->sp, &temp_u64b); // Read operand
		if (ret) break;
		ret = core_stack_read64(core, mem, core->sp + 8, &temp_u64); // Read operand
		if (ret) break;
		ret = core_stack_write64(core, mem, core->sp + 8, temp_u64b - temp_u64); // Write difference
		if (ret) break;
		core->sp += 8;
		core->pc += 1;
		break;
	case op_mul8:
		ret = core_stack_read8(core, mem, core->sp, &temp_u8b); // Read operand
		if (ret) break;
		ret = core_stack_read8(core, mem, core->sp + 1, &temp_u8); // Read operand
		if (ret) break;
		ret = core_stack_write8(core, mem, core->sp + 1, temp_u8 * temp_u8b); // Write product
		if (ret) break;
		core->sp += 1;
		core->pc += 1;
		break;
	case op_mul16:
		ret = core_stack_read16(core, mem, core->sp, &temp_u16b); // Read operand
		if (ret) break;
		ret = core_stack_read16(core, mem, core->sp + 2, &temp_u16); // Read operand
		if (ret) break;
		ret = core_stack_write16(core, mem, core->sp + 2, temp_u16 * temp_u16b); // Write product
		if (ret) break;
		core->sp += 2;
		core->pc += 1;
		break;
	case op_mul32:
		ret = core_stack_read32(core, mem, core->sp, &temp_u32b); // Read operand
		if (ret) break;
		ret = core_stack_read32(core, mem, core->sp + 4, &temp_u32); // Read operand
		if (ret) break;
		ret = core_stack_write32(core, mem, core->sp + 4, temp_u32 * temp_u32b); // Write product
		if (ret) break;
		core->sp += 4;
		core->pc += 1;
		break;
	case op_mul64:
		ret = core_stack_read64(core, mem, core->sp, &temp_u64b); // Read operand
		if (ret) break;
		ret = core_stack_read64(core, mem, core->sp + 8, &temp_u64); // Read operand
		if (ret) break;
		ret = core_stack_write64(core, mem, core->sp + 8, temp_u64 * temp_u64b); // Write product
		if (ret) break;
		core->sp += 8;
		core->pc += 1;
		break;
	case op_divu8:
		ret = core_stack_read8(core, mem, core->sp, &temp_u8b); // Read operand
		if (ret) break;
		ret = core_stack_read8(core, mem, core->sp + 1, &temp_u8); // Read operand
		if (ret) break;
		if (temp_u8b == 0) {
			ret = STINT_DIV_BY_ZERO;
			break;
		}
		ret = core_stack_write8(core, mem, core->sp + 1, temp_u8 / temp_u8b); // Write quotient
		if (ret) break;
		core->sp += 1;
		core->pc += 1;
		break;
	case op_divu16:
		ret = core_stack_read16(core, mem, core->sp, &temp_u16b); // Read operand
		if (ret) break;
		ret = core_stack_read16(core, mem, core->sp + 2, &temp_u16); // Read operand
		if (ret) break;
		if (temp_u16b == 0) {
			ret = STINT_DIV_BY_ZERO;
			break;
		}
		ret = core_stack_write16(core, mem, core->sp + 2, temp_u16 / temp_u16b); // Write quotient
		if (ret) break;
		core->sp += 2;
		core->pc += 1;
		break;
	case op_divu32:
		ret = core_stack_read32(core, mem, core->sp, &temp_u32b); // Read operand
		if (ret) break;
		ret = core_stack_read32(core, mem, core->sp + 4, &temp_u32); // Read operand
		if (ret) break;
		if (temp_u32b == 0) {
			ret = STINT_DIV_BY_ZERO;
			break;
		}
		ret = core_stack_write32(core, mem, core->sp + 4, temp_u32 / temp_u32b); // Write quotient
		if (ret) break;
		core->sp += 4;
		core->pc += 1;
		break;
	case op_divu64:
		ret = core_stack_read64(core, mem, core->sp, &temp_u64b); // Read operand
		if (ret) break;
		ret = core_stack_read64(core, mem, core->sp + 8, &temp_u64); // Read operand
		if (ret) break;
		if (temp_u64b == 0) {
			ret = STINT_DIV_BY_ZERO;
			break;
		}
		ret = core_stack_write64(core, mem, core->sp + 8, temp_u64 / temp_u64b); // Write quotient
		if (ret) break;
		core->sp += 8;
		core->pc += 1;
		break;
	case op_divru8:
		ret = core_stack_read8(core, mem, core->sp, &temp_u8b); // Read operand
		if (ret) break;
		ret = core_stack_read8(core, mem, core->sp + 1, &temp_u8); // Read operand
		if (ret) break;
		if (temp_u8 == 0) {
			ret = STINT_DIV_BY_ZERO;
			break;
		}
		ret = core_stack_write8(core, mem, core->sp + 1, temp_u8b / temp_u8); // Write quotient
		if (ret) break;
		core->sp += 1;
		core->pc += 1;
		break;
	case op_divru16:
		ret = core_stack_read16(core, mem, core->sp, &temp_u16b); // Read operand
		if (ret) break;
		ret = core_stack_read16(core, mem, core->sp + 2, &temp_u16); // Read operand
		if (ret) break;
		if (temp_u16 == 0) {
			ret = STINT_DIV_BY_ZERO;
			break;
		}
		ret = core_stack_write16(core, mem, core->sp + 2, temp_u16b / temp_u16); // Write quotient
		if (ret) break;
		core->sp += 2;
		core->pc += 1;
		break;
	case op_divru32:
		ret = core_stack_read32(core, mem, core->sp, &temp_u32b); // Read operand
		if (ret) break;
		ret = core_stack_read32(core, mem, core->sp + 4, &temp_u32); // Read operand
		if (ret) break;
		if (temp_u32 == 0) {
			ret = STINT_DIV_BY_ZERO;
			break;
		}
		ret = core_stack_write32(core, mem, core->sp + 4, temp_u32b / temp_u32); // Write quotient
		if (ret) break;
		core->sp += 4;
		core->pc += 1;
		break;
	case op_divru64:
		ret = core_stack_read64(core, mem, core->sp, &temp_u64b); // Read operand
		if (ret) break;
		ret = core_stack_read64(core, mem, core->sp + 8, &temp_u64); // Read operand
		if (ret) break;
		if (temp_u64 == 0) {
			ret = STINT_DIV_BY_ZERO;
			break;
		}
		ret = core_stack_write64(core, mem, core->sp + 8, temp_u64b / temp_u64); // Write quotient
		if (ret) break;
		core->sp += 8;
		core->pc += 1;
		break;
	case op_divi8:
		ret = core_stack_read8(core, mem, core->sp, &temp_u8b); // Read operand
		if (ret) break;
		ret = core_stack_read8(core, mem, core->sp + 1, &temp_u8); // Read operand
		if (ret) break;
		if (temp_u8b == 0) {
			ret = STINT_DIV_BY_ZERO;
			break;
		}
		ret = core_stack_write8(core, mem, core->sp + 1, (int8_t)temp_u8 / (int8_t)temp_u8b); // Write quotient
		if (ret) break;
		core->sp += 1;
		core->pc += 1;
		break;
	case op_divi16:
		ret = core_stack_read16(core, mem, core->sp, &temp_u16b); // Read operand
		if (ret) break;
		ret = core_stack_read16(core, mem, core->sp + 2, &temp_u16); // Read operand
		if (ret) break;
		if (temp_u16b == 0) {
			ret = STINT_DIV_BY_ZERO;
			break;
		}
		ret = core_stack_write16(core, mem, core->sp + 2, (int16_t)temp_u16 / (int16_t)temp_u16b); // Write quotient
		if (ret) break;
		core->sp += 2;
		core->pc += 1;
		break;
	case op_divi32:
		ret = core_stack_read32(core, mem, core->sp, &temp_u32b); // Read operand
		if (ret) break;
		ret = core_stack_read32(core, mem, core->sp + 4, &temp_u32); // Read operand
		if (ret) break;
		if (temp_u32b == 0) {
			ret = STINT_DIV_BY_ZERO;
			break;
		}
		ret = core_stack_write32(core, mem, core->sp + 4, (int32_t)temp_u32 / (int32_t)temp_u32b); // Write quotient
		if (ret) break;
		core->sp += 4;
		core->pc += 1;
		break;
	case op_divi64:
		ret = core_stack_read64(core, mem, core->sp, &temp_u64b); // Read operand
		if (ret) break;
		ret = core_stack_read64(core, mem, core->sp + 8, &temp_u64); // Read operand
		if (ret) break;
		if (temp_u64b == 0) {
			ret = STINT_DIV_BY_ZERO;
			break;
		}
		ret = core_stack_write64(core, mem, core->sp + 8, (int64_t)temp_u64 / (int64_t)temp_u64b); // Write quotient
		if (ret) break;
		core->sp += 8;
		core->pc += 1;
		break;
	case op_divri8:
		ret = core_stack_read8(core, mem, core->sp, &temp_u8b); // Read operand
		if (ret) break;
		ret = core_stack_read8(core, mem, core->sp + 1, &temp_u8); // Read operand
		if (ret) break;
		if (temp_u8 == 0) {
			ret = STINT_DIV_BY_ZERO;
			break;
		}
		ret = core_stack_write8(core, mem, core->sp + 1, (int8_t)temp_u8b / (int8_t)temp_u8); // Write quotient
		if (ret) break;
		core->sp += 1;
		core->pc += 1;
		break;
	case op_divri16:
		ret = core_stack_read16(core, mem, core->sp, &temp_u16b); // Read operand
		if (ret) break;
		ret = core_stack_read16(core, mem, core->sp + 2, &temp_u16); // Read operand
		if (ret) break;
		if (temp_u16 == 0) {
			ret = STINT_DIV_BY_ZERO;
			break;
		}
		ret = core_stack_write16(core, mem, core->sp + 2, (int16_t)temp_u16b / (int16_t)temp_u16); // Write quotient
		if (ret) break;
		core->sp += 2;
		core->pc += 1;
		break;
	case op_divri32:
		ret = core_stack_read32(core, mem, core->sp, &temp_u32b); // Read operand
		if (ret) break;
		ret = core_stack_read32(core, mem, core->sp + 4, &temp_u32); // Read operand
		if (ret) break;
		if (temp_u32 == 0) {
			ret = STINT_DIV_BY_ZERO;
			break;
		}
		ret = core_stack_write32(core, mem, core->sp + 4, (int32_t)temp_u32b / (int32_t)temp_u32); // Write quotient
		if (ret) break;
		core->sp += 4;
		core->pc += 1;
		break;
	case op_divri64:
		ret = core_stack_read64(core, mem, core->sp, &temp_u64b); // Read operand
		if (ret) break;
		ret = core_stack_read64(core, mem, core->sp + 8, &temp_u64); // Read operand
		if (ret) break;
		if (temp_u64 == 0) {
			ret = STINT_DIV_BY_ZERO;
			break;
		}
		ret = core_stack_write64(core, mem, core->sp + 8, (int64_t)temp_u64b / (int64_t)temp_u64); // Write quotient
		if (ret) break;
		core->sp += 8;
		core->pc += 1;
		break;
	case op_modu8:
		ret = core_stack_read8(core, mem, core->sp, &temp_u8b); // Read operand
		if (ret) break;
		ret = core_stack_read8(core, mem, core->sp + 1, &temp_u8); // Read operand
		if (ret) break;
		if (temp_u8b == 0) {
			ret = STINT_DIV_BY_ZERO;
			break;
		}
		ret = core_stack_write8(core, mem, core->sp + 1, temp_u8 % temp_u8b); // Write remainder
		if (ret) break;
		core->sp += 1;
		core->pc += 1;
		break;
	case op_modu16:
		ret = core_stack_read16(core, mem, core->sp, &temp_u16b); // Read operand
		if (ret) break;
		ret = core_stack_read16(core, mem, core->sp + 2, &temp_u16); // Read operand
		if (ret) break;
		if (temp_u16b == 0) {
			ret = STINT_DIV_BY_ZERO;
			break;
		}
		ret = core_stack_write16(core, mem, core->sp + 2, temp_u16 % temp_u16b); // Write remainder
		if (ret) break;
		core->sp += 2;
		core->pc += 1;
		break;
	case op_modu32:
		ret = core_stack_read32(core, mem, core->sp, &temp_u32b); // Read operand
		if (ret) break;
		ret = core_stack_read32(core, mem, core->sp + 4, &temp_u32); // Read operand
		if (ret) break;
		if (temp_u32b == 0) {
			ret = STINT_DIV_BY_ZERO;
			break;
		}
		ret = core_stack_write32(core, mem, core->sp + 4, temp_u32 % temp_u32b); // Write remainder
		if (ret) break;
		core->sp += 4;
		core->pc += 1;
		break;
	case op_modu64:
		ret = core_stack_read64(core, mem, core->sp, &temp_u64b); // Read operand
		if (ret) break;
		ret = core_stack_read64(core, mem, core->sp + 8, &temp_u64); // Read operand
		if (ret) break;
		if (temp_u64b == 0) {
			ret = STINT_DIV_BY_ZERO;
			break;
		}
		ret = core_stack_write64(core, mem, core->sp + 8, temp_u64 % temp_u64b); // Write remainder
		if (ret) break;
		core->sp += 8;
		core->pc += 1;
		break;
	case op_modru8:
		ret = core_stack_read8(core, mem, core->sp, &temp_u8b); // Read operand
		if (ret) break;
		ret = core_stack_read8(core, mem, core->sp + 1, &temp_u8); // Read operand
		if (ret) break;
		if (temp_u8 == 0) {
			ret = STINT_DIV_BY_ZERO;
			break;
		}
		ret = core_stack_write8(core, mem, core->sp + 1, temp_u8b % temp_u8); // Write remainder
		if (ret) break;
		core->sp += 1;
		core->pc += 1;
		break;
	case op_modru16:
		ret = core_stack_read16(core, mem, core->sp, &temp_u16b); // Read operand
		if (ret) break;
		ret = core_stack_read16(core, mem, core->sp + 2, &temp_u16); // Read operand
		if (ret) break;
		if (temp_u16 == 0) {
			ret = STINT_DIV_BY_ZERO;
			break;
		}
		ret = core_stack_write16(core, mem, core->sp + 2, temp_u16b % temp_u16); // Write remainder
		if (ret) break;
		core->sp += 2;
		core->pc += 1;
		break;
	case op_modru32:
		ret = core_stack_read32(core, mem, core->sp, &temp_u32b); // Read operand
		if (ret) break;
		ret = core_stack_read32(core, mem, core->sp + 4, &temp_u32); // Read operand
		if (ret) break;
		if (temp_u32 == 0) {
			ret = STINT_DIV_BY_ZERO;
			break;
		}
		ret = core_stack_write32(core, mem, core->sp + 4, temp_u32b % temp_u32); // Write remainder
		if (ret) break;
		core->sp += 4;
		core->pc += 1;
		break;
	case op_modru64:
		ret = core_stack_read64(core, mem, core->sp, &temp_u64b); // Read operand
		if (ret) break;
		ret = core_stack_read64(core, mem, core->sp + 8, &temp_u64); // Read operand
		if (ret) break;
		if (temp_u64 == 0) {
			ret = STINT_DIV_BY_ZERO;
			break;
		}
		ret = core_stack_write64(core, mem, core->sp + 8, temp_u64b % temp_u64); // Write remainder
		if (ret) break;
		core->sp += 8;
		core->pc += 1;
		break;
	case op_modi8:
		ret = core_stack_read8(core, mem, core->sp, &temp_u8b); // Read operand
		if (ret) break;
		ret = core_stack_read8(core, mem, core->sp + 1, &temp_u8); // Read operand
		if (ret) break;
		if (temp_u8b == 0) {
			ret = STINT_DIV_BY_ZERO;
			break;
		}
		ret = core_stack_write8(core, mem, core->sp + 1, (int8_t)temp_u8 % (int8_t)temp_u8b); // Write remainder
		if (ret) break;
		core->sp += 1;
		core->pc += 1;
		break;
	case op_modi16:
		ret = core_stack_read16(core, mem, core->sp, &temp_u16b); // Read operand
		if (ret) break;
		ret = core_stack_read16(core, mem, core->sp + 2, &temp_u16); // Read operand
		if (ret) break;
		if (temp_u16b == 0) {
			ret = STINT_DIV_BY_ZERO;
			break;
		}
		ret = core_stack_write16(core, mem, core->sp + 2, (int16_t)temp_u16 % (int16_t)temp_u16b); // Write remainder
		if (ret) break;
		core->sp += 2;
		core->pc += 1;
		break;
	case op_modi32:
		ret = core_stack_read32(core, mem, core->sp, &temp_u32b); // Read operand
		if (ret) break;
		ret = core_stack_read32(core, mem, core->sp + 4, &temp_u32); // Read operand
		if (ret) break;
		if (temp_u32b == 0) {
			ret = STINT_DIV_BY_ZERO;
			break;
		}
		ret = core_stack_write32(core, mem, core->sp + 4, (int32_t)temp_u32 % (int32_t)temp_u32b); // Write remainder
		if (ret) break;
		core->sp += 4;
		core->pc += 1;
		break;
	case op_modi64:
		ret = core_stack_read64(core, mem, core->sp, &temp_u64b); // Read operand
		if (ret) break;
		ret = core_stack_read64(core, mem, core->sp + 8, &temp_u64); // Read operand
		if (ret) break;
		if (temp_u64b == 0) {
			ret = STINT_DIV_BY_ZERO;
			break;
		}
		ret = core_stack_write64(core, mem, core->sp + 8, (int64_t)temp_u64 % (int64_t)temp_u64b); // Write remainder
		if (ret) break;
		core->sp += 8;
		core->pc += 1;
		break;
	case op_modri8:
		ret = core_stack_read8(core, mem, core->sp, &temp_u8b); // Read operand
		if (ret) break;
		ret = core_stack_read8(core, mem, core->sp + 1, &temp_u8); // Read operand
		if (ret) break;
		if (temp_u8 == 0) {
			ret = STINT_DIV_BY_ZERO;
			break;
		}
		ret = core_stack_write8(core, mem, core->sp + 1, (int8_t)temp_u8b % (int8_t)temp_u8); // Write remainder
		if (ret) break;
		core->sp += 1;
		core->pc += 1;
		break;
	case op_modri16:
		ret = core_stack_read16(core, mem, core->sp, &temp_u16b); // Read operand
		if (ret) break;
		ret = core_stack_read16(core, mem, core->sp + 2, &temp_u16); // Read operand
		if (ret) break;
		if (temp_u16 == 0) {
			ret = STINT_DIV_BY_ZERO;
			break;
		}
		ret = core_stack_write16(core, mem, core->sp + 2, (int16_t)temp_u16b % (int16_t)temp_u16); // Write remainder
		if (ret) break;
		core->sp += 2;
		core->pc += 1;
		break;
	case op_modri32:
		ret = core_stack_read32(core, mem, core->sp, &temp_u32b); // Read operand
		if (ret) break;
		ret = core_stack_read32(core, mem, core->sp + 4, &temp_u32); // Read operand
		if (ret) break;
		if (temp_u32 == 0) {
			ret = STINT_DIV_BY_ZERO;
			break;
		}
		ret = core_stack_write32(core, mem, core->sp + 4, (int32_t)temp_u32b % (int32_t)temp_u32); // Write remainder
		if (ret) break;
		core->sp += 4;
		core->pc += 1;
		break;
	case op_modri64:
		ret = core_stack_read64(core, mem, core->sp, &temp_u64b); // Read operand
		if (ret) break;
		ret = core_stack_read64(core, mem, core->sp + 8, &temp_u64); // Read operand
		if (ret) break;
		if (temp_u64 == 0) {
			ret = STINT_DIV_BY_ZERO;
			break;
		}
		ret = core_stack_write64(core, mem, core->sp + 8, (int64_t)temp_u64b % (int64_t)temp_u64); // Write remainder
		if (ret) break;
		core->sp += 8;
		core->pc += 1;
		break;

	//
	// Bitwise shift operations
	//
	case op_lshift8:
		ret = core_stack_read8(core, mem, core->sp, &temp_u8b); // Read operand
		if (ret) break;
		ret = core_stack_read8(core, mem, core->sp + 1, &temp_u8); // Read operand
		if (ret) break;
		ret = core_stack_write8(core, mem, core->sp + 1, temp_u8 << temp_u8b); // Write result
		if (ret) break;
		core->sp += 1;
		core->pc += 1;
		break;
	case op_lshift16:
		ret = core_stack_read8(core, mem, core->sp, &temp_u8b); // Read operand
		if (ret) break;
		ret = core_stack_read16(core, mem, core->sp + 1, &temp_u16); // Read operand
		if (ret) break;
		ret = core_stack_write16(core, mem, core->sp + 1, temp_u16 << temp_u8b); // Write result
		if (ret) break;
		core->sp += 1;
		core->pc += 1;
		break;
	case op_lshift32:
		ret = core_stack_read8(core, mem, core->sp, &temp_u8b); // Read operand
		if (ret) break;
		ret = core_stack_read32(core, mem, core->sp + 1, &temp_u32); // Read operand
		if (ret) break;
		ret = core_stack_write32(core, mem, core->sp + 1, temp_u32 << temp_u8b); // Write result
		if (ret) break;
		core->sp += 1;
		core->pc += 1;
		break;
	case op_lshift64:
		ret = core_stack_read8(core, mem, core->sp, &temp_u8b); // Read operand
		if (ret) break;
		ret = core_stack_read64(core, mem, core->sp + 1, &temp_u64); // Read operand
		if (ret) break;
		ret = core_stack_write64(core, mem, core->sp + 1, temp_u64 << temp_u8b); // Write result
		if (ret) break;
		core->sp += 1;
		core->pc += 1;
		break;
	case op_rshiftu8:
		ret = core_stack_read8(core, mem, core->sp, &temp_u8b); // Read operand
		if (ret) break;
		ret = core_stack_read8(core, mem, core->sp + 1, &temp_u8); // Read operand
		if (ret) break;
		ret = core_stack_write8(core, mem, core->sp + 1, temp_u8 >> temp_u8b); // Write result
		if (ret) break;
		core->sp += 1;
		core->pc += 1;
		break;
	case op_rshiftu16:
		ret = core_stack_read8(core, mem, core->sp, &temp_u8b); // Read operand
		if (ret) break;
		ret = core_stack_read16(core, mem, core->sp + 1, &temp_u16); // Read operand
		if (ret) break;
		ret = core_stack_write16(core, mem, core->sp + 1, temp_u16 >> temp_u8b); // Write result
		if (ret) break;
		core->sp += 1;
		core->pc += 1;
		break;
	case op_rshiftu32:
		ret = core_stack_read8(core, mem, core->sp, &temp_u8b); // Read operand
		if (ret) break;
		ret = core_stack_read32(core, mem, core->sp + 1, &temp_u32); // Read operand
		if (ret) break;
		ret = core_stack_write32(core, mem, core->sp + 1, temp_u32 >> temp_u8b); // Write result
		if (ret) break;
		core->sp += 1;
		core->pc += 1;
		break;
	case op_rshiftu64:
		ret = core_stack_read8(core, mem, core->sp, &temp_u8b); // Read operand
		if (ret) break;
		ret = core_stack_read64(core, mem, core->sp + 1, &temp_u64); // Read operand
		if (ret) break;
		ret = core_stack_write64(core, mem, core->sp + 1, temp_u64 >> temp_u8b); // Write result
		if (ret) break;
		core->sp += 1;
		core->pc += 1;
		break;
	case op_rshifti8:
		ret = core_stack_read8(core, mem, core->sp, &temp_u8b); // Read operand
		if (ret) break;
		ret = core_stack_read8(core, mem, core->sp + 1, &temp_u8); // Read operand
		if (ret) break;
		ret = core_stack_write8(core, mem, core->sp + 1, (int8_t)temp_u8 >> temp_u8b); // Write result
		if (ret) break;
		core->sp += 1;
		core->pc += 1;
		break;
	case op_rshifti16:
		ret = core_stack_read8(core, mem, core->sp, &temp_u8b); // Read operand
		if (ret) break;
		ret = core_stack_read16(core, mem, core->sp + 1, &temp_u16); // Read operand
		if (ret) break;
		ret = core_stack_write16(core, mem, core->sp + 1, (int16_t)temp_u16 >> temp_u8b); // Write result
		if (ret) break;
		core->sp += 1;
		core->pc += 1;
		break;
	case op_rshifti32:
		ret = core_stack_read8(core, mem, core->sp, &temp_u8b); // Read operand
		if (ret) break;
		ret = core_stack_read32(core, mem, core->sp + 1, &temp_u32); // Read operand
		if (ret) break;
		ret = core_stack_write32(core, mem, core->sp + 1, (int32_t)temp_u32 >> temp_u8b); // Write result
		if (ret) break;
		core->sp += 1;
		core->pc += 1;
		break;
	case op_rshifti64:
		ret = core_stack_read8(core, mem, core->sp, &temp_u8b); // Read operand
		if (ret) break;
		ret = core_stack_read64(core, mem, core->sp + 1, &temp_u64); // Read operand
		if (ret) break;
		ret = core_stack_write64(core, mem, core->sp + 1, (int64_t)temp_u64 >> temp_u8b); // Write result
		if (ret) break;
		core->sp += 1;
		core->pc += 1;
		break;

	//
	// Bitwise logical operations
	//
	case op_band8:
		ret = core_stack_read8(core, mem, core->sp, &temp_u8b); // Read operand
		if (ret) break;
		ret = core_stack_read8(core, mem, core->sp + 1, &temp_u8); // Read operand
		if (ret) break;
		ret = core_stack_write8(core, mem, core->sp + 1, temp_u8 & temp_u8b); // Write result
		if (ret) break;
		core->sp += 1;
		core->pc += 1;
		break;
	case op_band16:
		ret = core_stack_read16(core, mem, core->sp, &temp_u16b); // Read operand
		if (ret) break;
		ret = core_stack_read16(core, mem, core->sp + 2, &temp_u16); // Read operand
		if (ret) break;
		ret = core_stack_write16(core, mem, core->sp + 2, temp_u16 & temp_u16b); // Write result
		if (ret) break;
		core->sp += 2;
		core->pc += 1;
		break;
	case op_band32:
		ret = core_stack_read32(core, mem, core->sp, &temp_u32b); // Read operand
		if (ret) break;
		ret = core_stack_read32(core, mem, core->sp + 4, &temp_u32); // Read operand
		if (ret) break;
		ret = core_stack_write32(core, mem, core->sp + 4, temp_u32 & temp_u32b); // Write result
		if (ret) break;
		core->sp += 4;
		core->pc += 1;
		break;
	case op_band64:
		ret = core_stack_read64(core, mem, core->sp, &temp_u64b); // Read operand
		if (ret) break;
		ret = core_stack_read64(core, mem, core->sp + 8, &temp_u64); // Read operand
		if (ret) break;
		ret = core_stack_write64(core, mem, core->sp + 8, temp_u64 & temp_u64b); // Write result
		if (ret) break;
		core->sp += 8;
		core->pc += 1;
		break;
	case op_bor8:
		ret = core_stack_read8(core, mem, core->sp, &temp_u8b); // Read operand
		if (ret) break;
		ret = core_stack_read8(core, mem, core->sp + 1, &temp_u8); // Read operand
		if (ret) break;
		ret = core_stack_write8(core, mem, core->sp + 1, temp_u8 | temp_u8b); // Write result
		if (ret) break;
		core->sp += 1;
		core->pc += 1;
		break;
	case op_bor16:
		ret = core_stack_read16(core, mem, core->sp, &temp_u16b); // Read operand
		if (ret) break;
		ret = core_stack_read16(core, mem, core->sp + 2, &temp_u16); // Read operand
		if (ret) break;
		ret = core_stack_write16(core, mem, core->sp + 2, temp_u16 | temp_u16b); // Write result
		if (ret) break;
		core->sp += 2;
		core->pc += 1;
		break;
	case op_bor32:
		ret = core_stack_read32(core, mem, core->sp, &temp_u32b); // Read operand
		if (ret) break;
		ret = core_stack_read32(core, mem, core->sp + 4, &temp_u32); // Read operand
		if (ret) break;
		ret = core_stack_write32(core, mem, core->sp + 4, temp_u32 | temp_u32b); // Write result
		if (ret) break;
		core->sp += 4;
		core->pc += 1;
		break;
	case op_bor64:
		ret = core_stack_read64(core, mem, core->sp, &temp_u64b); // Read operand
		if (ret) break;
		ret = core_stack_read64(core, mem, core->sp + 8, &temp_u64); // Read operand
		if (ret) break;
		ret = core_stack_write64(core, mem, core->sp + 8, temp_u64 | temp_u64b); // Write result
		if (ret) break;
		core->sp += 8;
		core->pc += 1;
		break;
	case op_bxor8:
		ret = core_stack_read8(core, mem, core->sp, &temp_u8b); // Read operand
		if (ret) break;
		ret = core_stack_read8(core, mem, core->sp + 1, &temp_u8); // Read operand
		if (ret) break;
		ret = core_stack_write8(core, mem, core->sp + 1, temp_u8 ^ temp_u8b); // Write result
		if (ret) break;
		core->sp += 1;
		core->pc += 1;
		break;
	case op_bxor16:
		ret = core_stack_read16(core, mem, core->sp, &temp_u16b); // Read operand
		if (ret) break;
		ret = core_stack_read16(core, mem, core->sp + 2, &temp_u16); // Read operand
		if (ret) break;
		ret = core_stack_write16(core, mem, core->sp + 2, temp_u16 ^ temp_u16b); // Write result
		if (ret) break;
		core->sp += 2;
		core->pc += 1;
		break;
	case op_bxor32:
		ret = core_stack_read32(core, mem, core->sp, &temp_u32b); // Read operand
		if (ret) break;
		ret = core_stack_read32(core, mem, core->sp + 4, &temp_u32); // Read operand
		if (ret) break;
		ret = core_stack_write32(core, mem, core->sp + 4, temp_u32 ^ temp_u32b); // Write result
		if (ret) break;
		core->sp += 4;
		core->pc += 1;
		break;
	case op_bxor64:
		ret = core_stack_read64(core, mem, core->sp, &temp_u64b); // Read operand
		if (ret) break;
		ret = core_stack_read64(core, mem, core->sp + 8, &temp_u64); // Read operand
		if (ret) break;
		ret = core_stack_write64(core, mem, core->sp + 8, temp_u64 ^ temp_u64b); // Write result
		if (ret) break;
		core->sp += 8;
		core->pc += 1;
		break;
	case op_binv8:
		ret = core_stack_read8(core, mem, core->sp, &temp_u8); // Read operand
		if (ret) break;
		ret = core_stack_write8(core, mem, core->sp, ~temp_u8); // Write result
		if (ret) break;
		core->pc += 1;
		break;
	case op_binv16:
		ret = core_stack_read16(core, mem, core->sp, &temp_u16); // Read operand
		if (ret) break;
		ret = core_stack_write16(core, mem, core->sp, ~temp_u16); // Write result
		if (ret) break;
		core->pc += 1;
		break;
	case op_binv32:
		ret = core_stack_read32(core, mem, core->sp, &temp_u32); // Read operand
		if (ret) break;
		ret = core_stack_write32(core, mem, core->sp, ~temp_u32); // Write result
		if (ret) break;
		core->pc += 1;
		break;
	case op_binv64:
		ret = core_stack_read64(core, mem, core->sp, &temp_u64); // Read operand
		if (ret) break;
		ret = core_stack_write64(core, mem, core->sp, ~temp_u64); // Write result
		if (ret) break;
		core->pc += 1;
		break;

	//
	// Boolean logical operations
	//
	case op_land8:
		ret = core_stack_read8(core, mem, core->sp, &temp_u8b); // Read operand
		if (ret) break;
		ret = core_stack_read8(core, mem, core->sp + 1, &temp_u8); // Read operand
		if (ret) break;
		ret = core_stack_write8(core, mem, core->sp + 1, temp_u8 && temp_u8b); // Write result
		if (ret) break;
		core->sp += 1;
		core->pc += 1;
		break;
	case op_land16:
		ret = core_stack_read16(core, mem, core->sp, &temp_u16b); // Read operand
		if (ret) break;
		ret = core_stack_read16(core, mem, core->sp + 2, &temp_u16); // Read operand
		if (ret) break;
		ret = core_stack_write16(core, mem, core->sp + 2, temp_u16 && temp_u16b); // Write result
		if (ret) break;
		core->sp += 2;
		core->pc += 1;
		break;
	case op_land32:
		ret = core_stack_read32(core, mem, core->sp, &temp_u32b); // Read operand
		if (ret) break;
		ret = core_stack_read32(core, mem, core->sp + 4, &temp_u32); // Read operand
		if (ret) break;
		ret = core_stack_write32(core, mem, core->sp + 4, temp_u32 && temp_u32b); // Write result
		if (ret) break;
		core->sp += 4;
		core->pc += 1;
		break;
	case op_land64:
		ret = core_stack_read64(core, mem, core->sp, &temp_u64b); // Read operand
		if (ret) break;
		ret = core_stack_read64(core, mem, core->sp + 8, &temp_u64); // Read operand
		if (ret) break;
		ret = core_stack_write64(core, mem, core->sp + 8, temp_u64 && temp_u64b); // Write result
		if (ret) break;
		core->sp += 8;
		core->pc += 1;
		break;
	case op_lor8:
		ret = core_stack_read8(core, mem, core->sp, &temp_u8b); // Read operand
		if (ret) break;
		ret = core_stack_read8(core, mem, core->sp + 1, &temp_u8); // Read operand
		if (ret) break;
		ret = core_stack_write8(core, mem, core->sp + 1, temp_u8 || temp_u8b); // Write result
		if (ret) break;
		core->sp += 1;
		core->pc += 1;
		break;
	case op_lor16:
		ret = core_stack_read16(core, mem, core->sp, &temp_u16b); // Read operand
		if (ret) break;
		ret = core_stack_read16(core, mem, core->sp + 2, &temp_u16); // Read operand
		if (ret) break;
		ret = core_stack_write16(core, mem, core->sp + 2, temp_u16 || temp_u16b); // Write result
		if (ret) break;
		core->sp += 2;
		core->pc += 1;
		break;
	case op_lor32:
		ret = core_stack_read32(core, mem, core->sp, &temp_u32b); // Read operand
		if (ret) break;
		ret = core_stack_read32(core, mem, core->sp + 4, &temp_u32); // Read operand
		if (ret) break;
		ret = core_stack_write32(core, mem, core->sp + 4, temp_u32 || temp_u32b); // Write result
		if (ret) break;
		core->sp += 4;
		core->pc += 1;
		break;
	case op_lor64:
		ret = core_stack_read64(core, mem, core->sp, &temp_u64b); // Read operand
		if (ret) break;
		ret = core_stack_read64(core, mem, core->sp + 8, &temp_u64); // Read operand
		if (ret) break;
		ret = core_stack_write64(core, mem, core->sp + 8, temp_u64 || temp_u64b); // Write result
		if (ret) break;
		core->sp += 8;
		core->pc += 1;
		break;
	case op_linv8:
		ret = core_stack_read8(core, mem, core->sp, &temp_u8); // Read operand
		if (ret) break;
		ret = core_stack_write8(core, mem, core->sp, !temp_u8); // Write result
		if (ret) break;
		core->pc += 1;
		break;
	case op_linv16:
		ret = core_stack_read16(core, mem, core->sp, &temp_u16); // Read operand
		if (ret) break;
		ret = core_stack_write16(core, mem, core->sp, !temp_u16); // Write result
		if (ret) break;
		core->pc += 1;
		break;
	case op_linv32:
		ret = core_stack_read32(core, mem, core->sp, &temp_u32); // Read operand
		if (ret) break;
		ret = core_stack_write32(core, mem, core->sp, !temp_u32); // Write result
		if (ret) break;
		core->pc += 1;
		break;
	case op_linv64:
		ret = core_stack_read64(core, mem, core->sp, &temp_u64); // Read operand
		if (ret) break;
		ret = core_stack_write64(core, mem, core->sp, !temp_u64); // Write result
		if (ret) break;
		core->pc += 1;
		break;

	//
	// Comparison operations
	//
	case op_ceq8:
		ret = core_stack_read8(core, mem, core->sp, &temp_u8b); // Read operand
		if (ret) break;
		ret = core_stack_read8(core, mem, core->sp + 1, &temp_u8); // Read operand
		if (ret) break;
		ret = core_stack_write8(core, mem, core->sp + 1, temp_u8 == temp_u8b); // Write result
		if (ret) break;
		core->sp += 1;
		core->pc += 1;
		break;
	case op_ceq16:
		ret = core_stack_read16(core, mem, core->sp, &temp_u16b); // Read operand
		if (ret) break;
		ret = core_stack_read16(core, mem, core->sp + 2, &temp_u16); // Read operand
		if (ret) break;
		ret = core_stack_write16(core, mem, core->sp + 2, temp_u16 == temp_u16b); // Write result
		if (ret) break;
		core->sp += 2;
		core->pc += 1;
		break;
	case op_ceq32:
		ret = core_stack_read32(core, mem, core->sp, &temp_u32b); // Read operand
		if (ret) break;
		ret = core_stack_read32(core, mem, core->sp + 4, &temp_u32); // Read operand
		if (ret) break;
		ret = core_stack_write32(core, mem, core->sp + 4, temp_u32 == temp_u32b); // Write result
		if (ret) break;
		core->sp += 4;
		core->pc += 1;
		break;
	case op_ceq64:
		ret = core_stack_read64(core, mem, core->sp, &temp_u64b); // Read operand
		if (ret) break;
		ret = core_stack_read64(core, mem, core->sp + 8, &temp_u64); // Read operand
		if (ret) break;
		ret = core_stack_write64(core, mem, core->sp + 8, temp_u64 == temp_u64b); // Write result
		if (ret) break;
		core->sp += 8;
		core->pc += 1;
		break;
	case op_cne8:
		ret = core_stack_read8(core, mem, core->sp, &temp_u8b); // Read operand
		if (ret) break;
		ret = core_stack_read8(core, mem, core->sp + 1, &temp_u8); // Read operand
		if (ret) break;
		ret = core_stack_write8(core, mem, core->sp + 1, temp_u8 != temp_u8b); // Write result
		if (ret) break;
		core->sp += 1;
		core->pc += 1;
		break;
	case op_cne16:
		ret = core_stack_read16(core, mem, core->sp, &temp_u16b); // Read operand
		if (ret) break;
		ret = core_stack_read16(core, mem, core->sp + 2, &temp_u16); // Read operand
		if (ret) break;
		ret = core_stack_write16(core, mem, core->sp + 2, temp_u16 != temp_u16b); // Write result
		if (ret) break;
		core->sp += 2;
		core->pc += 1;
		break;
	case op_cne32:
		ret = core_stack_read32(core, mem, core->sp, &temp_u32b); // Read operand
		if (ret) break;
		ret = core_stack_read32(core, mem, core->sp + 4, &temp_u32); // Read operand
		if (ret) break;
		ret = core_stack_write32(core, mem, core->sp + 4, temp_u32 != temp_u32b); // Write result
		if (ret) break;
		core->sp += 4;
		core->pc += 1;
		break;
	case op_cne64:
		ret = core_stack_read64(core, mem, core->sp, &temp_u64b); // Read operand
		if (ret) break;
		ret = core_stack_read64(core, mem, core->sp + 8, &temp_u64); // Read operand
		if (ret) break;
		ret = core_stack_write64(core, mem, core->sp + 8, temp_u64 != temp_u64b); // Write result
		if (ret) break;
		core->sp += 8;
		core->pc += 1;
		break;
	case op_cgtu8:
		ret = core_stack_read8(core, mem, core->sp, &temp_u8b); // Read operand
		if (ret) break;
		ret = core_stack_read8(core, mem, core->sp + 1, &temp_u8); // Read operand
		if (ret) break;
		ret = core_stack_write8(core, mem, core->sp + 1, temp_u8 > temp_u8b); // Write result
		if (ret) break;
		core->sp += 1;
		core->pc += 1;
		break;
	case op_cgtu16:
		ret = core_stack_read16(core, mem, core->sp, &temp_u16b); // Read operand
		if (ret) break;
		ret = core_stack_read16(core, mem, core->sp + 2, &temp_u16); // Read operand
		if (ret) break;
		ret = core_stack_write16(core, mem, core->sp + 2, temp_u16 > temp_u16b); // Write result
		if (ret) break;
		core->sp += 2;
		core->pc += 1;
		break;
	case op_cgtu32:
		ret = core_stack_read32(core, mem, core->sp, &temp_u32b); // Read operand
		if (ret) break;
		ret = core_stack_read32(core, mem, core->sp + 4, &temp_u32); // Read operand
		if (ret) break;
		ret = core_stack_write32(core, mem, core->sp + 4, temp_u32 > temp_u32b); // Write result
		if (ret) break;
		core->sp += 4;
		core->pc += 1;
		break;
	case op_cgtu64:
		ret = core_stack_read64(core, mem, core->sp, &temp_u64b); // Read operand
		if (ret) break;
		ret = core_stack_read64(core, mem, core->sp + 8, &temp_u64); // Read operand
		if (ret) break;
		ret = core_stack_write64(core, mem, core->sp + 8, temp_u64 > temp_u64b); // Write result
		if (ret) break;
		core->sp += 8;
		core->pc += 1;
		break;
	case op_cgti8:
		ret = core_stack_read8(core, mem, core->sp, &temp_u8b); // Read operand
		if (ret) break;
		ret = core_stack_read8(core, mem, core->sp + 1, &temp_u8); // Read operand
		if (ret) break;
		ret = core_stack_write8(core, mem, core->sp + 1, (int8_t)temp_u8 > (int8_t)temp_u8b); // Write result
		if (ret) break;
		core->sp += 1;
		core->pc += 1;
		break;
	case op_cgti16:
		ret = core_stack_read16(core, mem, core->sp, &temp_u16b); // Read operand
		if (ret) break;
		ret = core_stack_read16(core, mem, core->sp + 2, &temp_u16); // Read operand
		if (ret) break;
		ret = core_stack_write16(core, mem, core->sp + 2, (int16_t)temp_u16 > (int16_t)temp_u16b); // Write result
		if (ret) break;
		core->sp += 2;
		core->pc += 1;
		break;
	case op_cgti32:
		ret = core_stack_read32(core, mem, core->sp, &temp_u32b); // Read operand
		if (ret) break;
		ret = core_stack_read32(core, mem, core->sp + 4, &temp_u32); // Read operand
		if (ret) break;
		ret = core_stack_write32(core, mem, core->sp + 4, (int32_t)temp_u32 > (int32_t)temp_u32b); // Write result
		if (ret) break;
		core->sp += 4;
		core->pc += 1;
		break;
	case op_cgti64:
		ret = core_stack_read64(core, mem, core->sp, &temp_u64b); // Read operand
		if (ret) break;
		ret = core_stack_read64(core, mem, core->sp + 8, &temp_u64); // Read operand
		if (ret) break;
		ret = core_stack_write64(core, mem, core->sp + 8, (int64_t)temp_u64 > (int64_t)temp_u64b); // Write result
		if (ret) break;
		core->sp += 8;
		core->pc += 1;
		break;
	case op_cltu8:
		ret = core_stack_read8(core, mem, core->sp, &temp_u8b); // Read operand
		if (ret) break;
		ret = core_stack_read8(core, mem, core->sp + 1, &temp_u8); // Read operand
		if (ret) break;
		ret = core_stack_write8(core, mem, core->sp + 1, temp_u8 < temp_u8b); // Write result
		if (ret) break;
		core->sp += 1;
		core->pc += 1;
		break;
	case op_cltu16:
		ret = core_stack_read16(core, mem, core->sp, &temp_u16b); // Read operand
		if (ret) break;
		ret = core_stack_read16(core, mem, core->sp + 2, &temp_u16); // Read operand
		if (ret) break;
		ret = core_stack_write16(core, mem, core->sp + 2, temp_u16 < temp_u16b); // Write result
		if (ret) break;
		core->sp += 2;
		core->pc += 1;
		break;
	case op_cltu32:
		ret = core_stack_read32(core, mem, core->sp, &temp_u32b); // Read operand
		if (ret) break;
		ret = core_stack_read32(core, mem, core->sp + 4, &temp_u32); // Read operand
		if (ret) break;
		ret = core_stack_write32(core, mem, core->sp + 4, temp_u32 < temp_u32b); // Write result
		if (ret) break;
		core->sp += 4;
		core->pc += 1;
		break;
	case op_cltu64:
		ret = core_stack_read64(core, mem, core->sp, &temp_u64b); // Read operand
		if (ret) break;
		ret = core_stack_read64(core, mem, core->sp + 8, &temp_u64); // Read operand
		if (ret) break;
		ret = core_stack_write64(core, mem, core->sp + 8, temp_u64 < temp_u64b); // Write result
		if (ret) break;
		core->sp += 8;
		core->pc += 1;
		break;
	case op_clti8:
		ret = core_stack_read8(core, mem, core->sp, &temp_u8b); // Read operand
		if (ret) break;
		ret = core_stack_read8(core, mem, core->sp + 1, &temp_u8); // Read operand
		if (ret) break;
		ret = core_stack_write8(core, mem, core->sp + 1, (int8_t)temp_u8 < (int8_t)temp_u8b); // Write result
		if (ret) break;
		core->sp += 1;
		core->pc += 1;
		break;
	case op_clti16:
		ret = core_stack_read16(core, mem, core->sp, &temp_u16b); // Read operand
		if (ret) break;
		ret = core_stack_read16(core, mem, core->sp + 2, &temp_u16); // Read operand
		if (ret) break;
		ret = core_stack_write16(core, mem, core->sp + 2, (int16_t)temp_u16 < (int16_t)temp_u16b); // Write result
		if (ret) break;
		core->sp += 2;
		core->pc += 1;
		break;
	case op_clti32:
		ret = core_stack_read32(core, mem, core->sp, &temp_u32b); // Read operand
		if (ret) break;
		ret = core_stack_read32(core, mem, core->sp + 4, &temp_u32); // Read operand
		if (ret) break;
		ret = core_stack_write32(core, mem, core->sp + 4, (int32_t)temp_u32 < (int32_t)temp_u32b); // Write result
		if (ret) break;
		core->sp += 4;
		core->pc += 1;
		break;
	case op_clti64:
		ret = core_stack_read64(core, mem, core->sp, &temp_u64b); // Read operand
		if (ret) break;
		ret = core_stack_read64(core, mem, core->sp + 8, &temp_u64); // Read operand
		if (ret) break;
		ret = core_stack_write64(core, mem, core->sp + 8, (int64_t)temp_u64 < (int64_t)temp_u64b); // Write result
		if (ret) break;
		core->sp += 8;
		core->pc += 1;
		break;
	case op_cgeu8:
		ret = core_stack_read8(core, mem, core->sp, &temp_u8b); // Read operand
		if (ret) break;
		ret = core_stack_read8(core, mem, core->sp + 1, &temp_u8); // Read operand
		if (ret) break;
		ret = core_stack_write8(core, mem, core->sp + 1, temp_u8 >= temp_u8b); // Write result
		if (ret) break;
		core->sp += 1;
		core->pc += 1;
		break;
	case op_cgeu16:
		ret = core_stack_read16(core, mem, core->sp, &temp_u16b); // Read operand
		if (ret) break;
		ret = core_stack_read16(core, mem, core->sp + 2, &temp_u16); // Read operand
		if (ret) break;
		ret = core_stack_write16(core, mem, core->sp + 2, temp_u16 >= temp_u16b); // Write result
		if (ret) break;
		core->sp += 2;
		core->pc += 1;
		break;
	case op_cgeu32:
		ret = core_stack_read32(core, mem, core->sp, &temp_u32b); // Read operand
		if (ret) break;
		ret = core_stack_read32(core, mem, core->sp + 4, &temp_u32); // Read operand
		if (ret) break;
		ret = core_stack_write32(core, mem, core->sp + 4, temp_u32 >= temp_u32b); // Write result
		if (ret) break;
		core->sp += 4;
		core->pc += 1;
		break;
	case op_cgeu64:
		ret = core_stack_read64(core, mem, core->sp, &temp_u64b); // Read operand
		if (ret) break;
		ret = core_stack_read64(core, mem, core->sp + 8, &temp_u64); // Read operand
		if (ret) break;
		ret = core_stack_write64(core, mem, core->sp + 8, temp_u64 >= temp_u64b); // Write result
		if (ret) break;
		core->sp += 8;
		core->pc += 1;
		break;
	case op_cgei8:
		ret = core_stack_read8(core, mem, core->sp, &temp_u8b); // Read operand
		if (ret) break;
		ret = core_stack_read8(core, mem, core->sp + 1, &temp_u8); // Read operand
		if (ret) break;
		ret = core_stack_write8(core, mem, core->sp + 1, (int8_t)temp_u8 >= (int8_t)temp_u8b); // Write result
		if (ret) break;
		core->sp += 1;
		core->pc += 1;
		break;
	case op_cgei16:
		ret = core_stack_read16(core, mem, core->sp, &temp_u16b); // Read operand
		if (ret) break;
		ret = core_stack_read16(core, mem, core->sp + 2, &temp_u16); // Read operand
		if (ret) break;
		ret = core_stack_write16(core, mem, core->sp + 2, (int16_t)temp_u16 >= (int16_t)temp_u16b); // Write result
		if (ret) break;
		core->sp += 2;
		core->pc += 1;
		break;
	case op_cgei32:
		ret = core_stack_read32(core, mem, core->sp, &temp_u32b); // Read operand
		if (ret) break;
		ret = core_stack_read32(core, mem, core->sp + 4, &temp_u32); // Read operand
		if (ret) break;
		ret = core_stack_write32(core, mem, core->sp + 4, (int32_t)temp_u32 >= (int32_t)temp_u32b); // Write result
		if (ret) break;
		core->sp += 4;
		core->pc += 1;
		break;
	case op_cgei64:
		ret = core_stack_read64(core, mem, core->sp, &temp_u64b); // Read operand
		if (ret) break;
		ret = core_stack_read64(core, mem, core->sp + 8, &temp_u64); // Read operand
		if (ret) break;
		ret = core_stack_write64(core, mem, core->sp + 8, (int64_t)temp_u64 >= (int64_t)temp_u64b); // Write result
		if (ret) break;
		core->sp += 8;
		core->pc += 1;
		break;
	case op_cleu8:
		ret = core_stack_read8(core, mem, core->sp, &temp_u8b); // Read operand
		if (ret) break;
		ret = core_stack_read8(core, mem, core->sp + 1, &temp_u8); // Read operand
		if (ret) break;
		ret = core_stack_write8(core, mem, core->sp + 1, temp_u8 <= temp_u8b); // Write result
		if (ret) break;
		core->sp += 1;
		core->pc += 1;
		break;
	case op_cleu16:
		ret = core_stack_read16(core, mem, core->sp, &temp_u16b); // Read operand
		if (ret) break;
		ret = core_stack_read16(core, mem, core->sp + 2, &temp_u16); // Read operand
		if (ret) break;
		ret = core_stack_write16(core, mem, core->sp + 2, temp_u16 <= temp_u16b); // Write result
		if (ret) break;
		core->sp += 2;
		core->pc += 1;
		break;
	case op_cleu32:
		ret = core_stack_read32(core, mem, core->sp, &temp_u32b); // Read operand
		if (ret) break;
		ret = core_stack_read32(core, mem, core->sp + 4, &temp_u32); // Read operand
		if (ret) break;
		ret = core_stack_write32(core, mem, core->sp + 4, temp_u32 <= temp_u32b); // Write result
		if (ret) break;
		core->sp += 4;
		core->pc += 1;
		break;
	case op_cleu64:
		ret = core_stack_read64(core, mem, core->sp, &temp_u64b); // Read operand
		if (ret) break;
		ret = core_stack_read64(core, mem, core->sp + 8, &temp_u64); // Read operand
		if (ret) break;
		ret = core_stack_write64(core, mem, core->sp + 8, temp_u64 <= temp_u64b); // Write result
		if (ret) break;
		core->sp += 8;
		core->pc += 1;
		break;
	case op_clei8:
		ret = core_stack_read8(core, mem, core->sp, &temp_u8b); // Read operand
		if (ret) break;
		ret = core_stack_read8(core, mem, core->sp + 1, &temp_u8); // Read operand
		if (ret) break;
		ret = core_stack_write8(core, mem, core->sp + 1, (int8_t)temp_u8 <= (int8_t)temp_u8b); // Write result
		if (ret) break;
		core->sp += 1;
		core->pc += 1;
		break;
	case op_clei16:
		ret = core_stack_read16(core, mem, core->sp, &temp_u16b); // Read operand
		if (ret) break;
		ret = core_stack_read16(core, mem, core->sp + 2, &temp_u16); // Read operand
		if (ret) break;
		ret = core_stack_write16(core, mem, core->sp + 2, (int16_t)temp_u16 <= (int16_t)temp_u16b); // Write result
		if (ret) break;
		core->sp += 2;
		core->pc += 1;
		break;
	case op_clei32:
		ret = core_stack_read32(core, mem, core->sp, &temp_u32b); // Read operand
		if (ret) break;
		ret = core_stack_read32(core, mem, core->sp + 4, &temp_u32); // Read operand
		if (ret) break;
		ret = core_stack_write32(core, mem, core->sp + 4, (int32_t)temp_u32 <= (int32_t)temp_u32b); // Write result
		if (ret) break;
		core->sp += 4;
		core->pc += 1;
		break;
	case op_clei64:
		ret = core_stack_read64(core, mem, core->sp, &temp_u64b); // Read operand
		if (ret) break;
		ret = core_stack_read64(core, mem, core->sp + 8, &temp_u64); // Read operand
		if (ret) break;
		ret = core_stack_write64(core, mem, core->sp + 8, (int64_t)temp_u64 <= (int64_t)temp_u64b); // Write result
		if (ret) break;
		core->sp += 8;
		core->pc += 1;
		break;

	//
	// Function operations
	//
	case op_call:
		ret = core_mem_read64(core, mem, core->pc + 1, &temp_u64); // Read imm address
		if (ret) break;
		ret = core_stack_write64(core, mem, core->sp - 8, core->sfp); // Push SFP
		if (ret) break;
		ret = core_stack_write64(core, mem, core->sp - 16, core->pc + 9); // Push RETA
		if (ret) break;
		core->sp -= 16;
		core->sfp = core->sp;
		core->pc = temp_u64;
		break;
	case op_calls:
		ret = core_stack_read64(core, mem, core->sp, &temp_u64); // Read and pop address
		if (ret) break;
		ret = core_stack_write64(core, mem, core->sp, core->sfp); // Push SFP
		if (ret) break;
		ret = core_stack_write64(core, mem, core->sp - 8, core->pc + 1); // Push RETA
		if (ret) break;
		core->sp -= 8;
		core->sfp = core->sp;
		core->pc = temp_u64;
		break;
	case op_ret:
		// We check the stack bounds ourselves so we can call the memory functions that bypass frame data access checks
		if (core->sfp >= core->sbp - 15 || core->sfp < core->slp || core->sfp < END_IO_ADDR) {
			ret = STINT_BAD_STACK_ACCESS;
			break;
		}
		ret = mem_read64(mem, core->sfp + 8, &temp_u64); // Read PSFP
		if (ret) break;
		temp_u64b = core->sfp;
		core->sfp = temp_u64; // Adjust SFP
		ret = mem_read64(mem, temp_u64b, &temp_u64); // Read RETA
		if (ret) {
			core->sfp = temp_u64b; // Restore original SFP on interrupt
			break;
		}
		core->sp = temp_u64b + 16;
		core->pc = temp_u64;
		break;

	//
	// Branching operations
	//
	case op_jmp:
		ret = core_mem_read64(core, mem, core->pc + 1, &temp_u64); // Read imm address
		if (ret) break;
		core->pc = temp_u64;
		break;
	case op_jmps:
		ret = core_stack_read64(core, mem, core->sp, &temp_u64); // Read and pop address
		if (ret) break;
		core->sp += 8;
		core->pc = temp_u64;
		break;
	case op_rjmpi8:
		ret = core_mem_read8(core, mem, core->pc + 1, &temp_u8); // Read operand
		if (ret) break;
		core->pc += (int8_t)temp_u8;
		break;
	case op_rjmpi16:
		ret = core_mem_read16(core, mem, core->pc + 1, &temp_u16); // Read operand
		if (ret) break;
		core->pc += (int16_t)temp_u16;
		break;
	case op_rjmpi32:
		ret = core_mem_read32(core, mem, core->pc + 1, &temp_u32); // Read operand
		if (ret) break;
		core->pc += (int32_t)temp_u32;
		break;

	//
	// Conditional branching operations
	//
	case op_brz8:
		ret = core_mem_read64(core, mem, core->pc + 1, &temp_addr); // Read address
		if (ret) break;
		ret = core_stack_read8(core, mem, core->sp, &temp_u8); // Read condition
		if (ret) break;
		if (temp_u8) {
			core->pc += 9;
		}
		else {
			core->pc = temp_addr;
		}
		core->sp += 1;
		break;
	case op_brz16:
		ret = core_mem_read64(core, mem, core->pc + 1, &temp_addr); // Read address
		if (ret) break;
		ret = core_stack_read16(core, mem, core->sp, &temp_u16); // Read condition
		if (ret) break;
		if (temp_u16) {
			core->pc += 9;
		}
		else {
			core->pc = temp_addr;
		}
		core->sp += 2;
		break;
	case op_brz32:
		ret = core_mem_read64(core, mem, core->pc + 1, &temp_addr); // Read address
		if (ret) break;
		ret = core_stack_read32(core, mem, core->sp, &temp_u32); // Read condition
		if (ret) break;
		if (temp_u32) {
			core->pc += 9;
		}
		else {
			core->pc = temp_addr;
		}
		core->sp += 4;
		break;
	case op_brz64:
		ret = core_mem_read64(core, mem, core->pc + 1, &temp_addr); // Read address
		if (ret) break;
		ret = core_stack_read64(core, mem, core->sp, &temp_u64); // Read condition
		if (ret) break;
		if (temp_u64) {
			core->pc += 9;
		}
		else {
			core->pc = temp_addr;
		}
		core->sp += 8;
		break;
	case op_rbrz8i8:
		ret = core_mem_read8(core, mem, core->pc + 1, &temp_u8b); // Read offset
		if (ret) break;
		ret = core_stack_read8(core, mem, core->sp, &temp_u8); // Read condition
		if (ret) break;
		core->sp += 1;
		if (temp_u8) {
			core->pc += 2;
		}
		else {
			core->pc += (int8_t)temp_u8b;
		}
		break;
	case op_rbrz8i16:
		ret = core_mem_read16(core, mem, core->pc + 1, &temp_u16); // Read offset
		if (ret) break;
		ret = core_stack_read8(core, mem, core->sp, &temp_u8); // Read condition
		if (ret) break;
		core->sp += 1;
		if (temp_u8) {
			core->pc += 3;
		}
		else {
			core->pc += (int16_t)temp_u16;
		}
		break;
	case op_rbrz8i32:
		ret = core_mem_read32(core, mem, core->pc + 1, &temp_u32); // Read offset
		if (ret) break;
		ret = core_stack_read8(core, mem, core->sp, &temp_u8); // Read condition
		if (ret) break;
		core->sp += 1;
		if (temp_u8) {
			core->pc += 5;
		}
		else {
			core->pc += (int32_t)temp_u32;
		}
		break;
	case op_rbrz16i8:
		ret = core_mem_read8(core, mem, core->pc + 1, &temp_u8); // Read offset
		if (ret) break;
		ret = core_stack_read16(core, mem, core->sp, &temp_u16); // Read condition
		if (ret) break;
		core->sp += 2;
		if (temp_u16) {
			core->pc += 2;
		}
		else {
			core->pc += (int8_t)temp_u8;
		}
		break;
	case op_rbrz16i16:
		ret = core_mem_read16(core, mem, core->pc + 1, &temp_u16); // Read offset
		if (ret) break;
		ret = core_stack_read16(core, mem, core->sp, &temp_u16b); // Read condition
		if (ret) break;
		core->sp += 2;
		if (temp_u16b) {
			core->pc += 3;
		}
		else {
			core->pc += (int16_t)temp_u16;
		}
		break;
	case op_rbrz16i32:
		ret = core_mem_read32(core, mem, core->pc + 1, &temp_u32); // Read offset
		if (ret) break;
		ret = core_stack_read16(core, mem, core->sp, &temp_u16); // Read condition
		if (ret) break;
		core->sp += 2;
		if (temp_u16) {
			core->pc += 5;
		}
		else {
			core->pc += (int32_t)temp_u32;
		}
		break;
	case op_rbrz32i8:
		ret = core_mem_read8(core, mem, core->pc + 1, &temp_u8); // Read offset
		if (ret) break;
		ret = core_stack_read32(core, mem, core->sp, &temp_u32); // Read condition
		if (ret) break;
		core->sp += 4;
		if (temp_u32) {
			core->pc += 2;
		}
		else {
			core->pc += (int8_t)temp_u8;
		}
		break;
	case op_rbrz32i16:
		ret = core_mem_read16(core, mem, core->pc + 1, &temp_u16); // Read offset
		if (ret) break;
		ret = core_stack_read32(core, mem, core->sp, &temp_u32); // Read condition
		if (ret) break;
		core->sp += 4;
		if (temp_u32) {
			core->pc += 3;
		}
		else {
			core->pc += (int16_t)temp_u16;
		}
		break;
	case op_rbrz32i32:
		ret = core_mem_read32(core, mem, core->pc + 1, &temp_u32); // Read offset
		if (ret) break;
		ret = core_stack_read32(core, mem, core->sp, &temp_u32b); // Read condition
		if (ret) break;
		core->sp += 4;
		if (temp_u32b) {
			core->pc += 5;
		}
		else {
			core->pc += (int32_t)temp_u32;
		}
		break;
	case op_rbrz64i8:
		ret = core_mem_read8(core, mem, core->pc + 1, &temp_u8); // Read offset
		if (ret) break;
		ret = core_stack_read64(core, mem, core->sp, &temp_u64); // Read condition
		if (ret) break;
		core->sp += 8;
		if (temp_u64) {
			core->pc += 2;
		}
		else {
			core->pc += (int8_t)temp_u8;
		}
		break;
	case op_rbrz64i16:
		ret = core_mem_read16(core, mem, core->pc + 1, &temp_u16); // Read offset
		if (ret) break;
		ret = core_stack_read64(core, mem, core->sp, &temp_u64); // Read condition
		if (ret) break;
		core->sp += 8;
		if (temp_u64) {
			core->pc += 3;
		}
		else {
			core->pc += (int16_t)temp_u16;
		}
		break;
	case op_rbrz64i32:
		ret = core_mem_read32(core, mem, core->pc + 1, &temp_u32); // Read offset
		if (ret) break;
		ret = core_stack_read64(core, mem, core->sp, &temp_u64); // Read condition
		if (ret) break;
		core->sp += 8;
		if (temp_u64) {
			core->pc += 5;
		}
		else {
			core->pc += (int32_t)temp_u32;
		}
		break;

	//
	// Memory operations
	//
	case op_load8:
		ret = core_stack_read64(core, mem, core->sp, &temp_addr); // Read addr
		if (ret) break;
		ret = core_mem_read8(core, mem, temp_addr, &temp_u8); // Read data
		if (ret) break;
		ret = core_stack_write8(core, mem, core->sp - 1, temp_u8); // Write to stack
		if (ret) break;
		core->sp -= 1;
		core->pc += 1;
		break;
	case op_load16:
		ret = core_stack_read64(core, mem, core->sp, &temp_addr); // Read addr
		if (ret) break;
		ret = core_mem_read16(core, mem, temp_addr, &temp_u16); // Read data
		if (ret) break;
		ret = core_stack_write16(core, mem, core->sp - 2, temp_u16); // Write to stack
		if (ret) break;
		core->sp -= 2;
		core->pc += 1;
		break;
	case op_load32:
		ret = core_stack_read64(core, mem, core->sp, &temp_addr); // Read addr
		if (ret) break;
		ret = core_mem_read32(core, mem, temp_addr, &temp_u32); // Read data
		if (ret) break;
		ret = core_stack_write32(core, mem, core->sp - 4, temp_u32); // Write to stack
		if (ret) break;
		core->sp -= 4;
		core->pc += 1;
		break;
	case op_load64:
		ret = core_stack_read64(core, mem, core->sp, &temp_addr); // Read addr
		if (ret) break;
		ret = core_mem_read64(core, mem, temp_addr, &temp_u64); // Read data
		if (ret) break;
		ret = core_stack_write64(core, mem, core->sp - 8, temp_u64); // Write to stack
		if (ret) break;
		core->sp -= 8;
		core->pc += 1;
		break;
	case op_loadpop8:
		ret = core_stack_read64(core, mem, core->sp, &temp_addr); // Read addr
		if (ret) break;
		ret = core_mem_read8(core, mem, temp_addr, &temp_u8); // Read data
		if (ret) break;
		ret = core_stack_write8(core, mem, core->sp + 7, temp_u8); // Write to stack
		if (ret) break;
		core->sp += 7;
		core->pc += 1;
		break;
	case op_loadpop16:
		ret = core_stack_read64(core, mem, core->sp, &temp_addr); // Read addr
		if (ret) break;
		ret = core_mem_read16(core, mem, temp_addr, &temp_u16); // Read data
		if (ret) break;
		ret = core_stack_write16(core, mem, core->sp + 6, temp_u16); // Write to stack
		if (ret) break;
		core->sp += 6;
		core->pc += 1;
		break;
	case op_loadpop32:
		ret = core_stack_read64(core, mem, core->sp, &temp_addr); // Read addr
		if (ret) break;
		ret = core_mem_read32(core, mem, temp_addr, &temp_u32); // Read data
		if (ret) break;
		ret = core_stack_write32(core, mem, core->sp + 4, temp_u32); // Write to stack
		if (ret) break;
		core->sp += 4;
		core->pc += 1;
		break;
	case op_loadpop64:
		ret = core_stack_read64(core, mem, core->sp, &temp_addr); // Read addr
		if (ret) break;
		ret = core_mem_read64(core, mem, temp_addr, &temp_u64); // Read data
		if (ret) break;
		ret = core_stack_write64(core, mem, core->sp, temp_u64); // Write to stack
		if (ret) break;
		core->pc += 1;
		break;
	case op_loadsfp8:
		ret = core_stack_read64(core, mem, core->sp, &temp_u64); // Read offset
		if (ret) break;
		ret = core_stack_read8(core, mem, core->sfp + (int64_t)temp_u64, &temp_u8); // Read data
		if (ret) break;
		ret = core_stack_write8(core, mem, core->sp - 1, temp_u8); // Write to stack
		if (ret) break;
		core->sp -= 1;
		core->pc += 1;
		break;
	case op_loadsfp16:
		ret = core_stack_read64(core, mem, core->sp, &temp_u64); // Read offset
		if (ret) break;
		ret = core_stack_read16(core, mem, core->sfp + (int64_t)temp_u64, &temp_u16); // Read data
		if (ret) break;
		ret = core_stack_write16(core, mem, core->sp - 2, temp_u16); // Write to stack
		if (ret) break;
		core->sp -= 2;
		core->pc += 1;
		break;
	case op_loadsfp32:
		ret = core_stack_read64(core, mem, core->sp, &temp_u64); // Read offset
		if (ret) break;
		ret = core_stack_read32(core, mem, core->sfp + (int64_t)temp_u64, &temp_u32); // Read data
		if (ret) break;
		ret = core_stack_write32(core, mem, core->sp - 4, temp_u32); // Write to stack
		if (ret) break;
		core->sp -= 4;
		core->pc += 1;
		break;
	case op_loadsfp64:
		ret = core_stack_read64(core, mem, core->sp, &temp_u64); // Read offset
		if (ret) break;
		ret = core_stack_read64(core, mem, core->sfp + (int64_t)temp_u64, &temp_u64b); // Read data
		if (ret) break;
		ret = core_stack_write64(core, mem, core->sp - 8, temp_u64b); // Write to stack
		if (ret) break;
		core->sp -= 8;
		core->pc += 1;
		break;
	case op_loadpopsfp8:
		ret = core_stack_read64(core, mem, core->sp, &temp_u64); // Read offset
		if (ret) break;
		ret = core_stack_read8(core, mem, core->sfp + (int64_t)temp_u64, &temp_u8); // Read data
		if (ret) break;
		ret = core_stack_write8(core, mem, core->sp + 7, temp_u8); // Write to stack
		if (ret) break;
		core->sp += 7;
		core->pc += 1;
		break;
	case op_loadpopsfp16:
		ret = core_stack_read64(core, mem, core->sp, &temp_u64); // Read offset
		if (ret) break;
		ret = core_stack_read16(core, mem, core->sfp + (int64_t)temp_u64, &temp_u16); // Read data
		if (ret) break;
		ret = core_stack_write16(core, mem, core->sp + 6, temp_u16); // Write to stack
		if (ret) break;
		core->sp += 6;
		core->pc += 1;
		break;
	case op_loadpopsfp32:
		ret = core_stack_read64(core, mem, core->sp, &temp_u64); // Read offset
		if (ret) break;
		ret = core_stack_read32(core, mem, core->sfp + (int64_t)temp_u64, &temp_u32); // Read data
		if (ret) break;
		ret = core_stack_write32(core, mem, core->sp + 4, temp_u32); // Write to stack
		if (ret) break;
		core->sp += 4;
		core->pc += 1;
		break;
	case op_loadpopsfp64:
		ret = core_stack_read64(core, mem, core->sp, &temp_u64); // Read offset
		if (ret) break;
		ret = core_stack_read64(core, mem, core->sfp + (int64_t)temp_u64, &temp_u64b); // Read data
		if (ret) break;
		ret = core_stack_write64(core, mem, core->sp, temp_u64b); // Write to stack
		if (ret) break;
		core->pc += 1;
		break;
	case op_store8:
		ret = core_stack_read64(core, mem, core->sp + 1, &temp_addr); // Read addr
		if (ret) break;
		ret = core_stack_read8(core, mem, core->sp, &temp_u8); // Read data
		if (ret) break;
		ret = core_mem_write8(core, mem, temp_addr, temp_u8); // Write to memory
		if (ret) break;
		core->pc += 1;
		break;
	case op_store16:
		ret = core_stack_read64(core, mem, core->sp + 2, &temp_addr); // Read addr
		if (ret) break;
		ret = core_stack_read16(core, mem, core->sp, &temp_u16); // Read data
		if (ret) break;
		ret = core_mem_write16(core, mem, temp_addr, temp_u16); // Write to memory
		if (ret) break;
		core->pc += 1;
		break;
	case op_store32:
		ret = core_stack_read64(core, mem, core->sp + 4, &temp_addr); // Read addr
		if (ret) break;
		ret = core_stack_read32(core, mem, core->sp, &temp_u32); // Read data
		if (ret) break;
		ret = core_mem_write32(core, mem, temp_addr, temp_u32); // Write to memory
		if (ret) break;
		core->pc += 1;
		break;
	case op_store64:
		ret = core_stack_read64(core, mem, core->sp + 8, &temp_addr); // Read addr
		if (ret) break;
		ret = core_stack_read64(core, mem, core->sp, &temp_u64); // Read data
		if (ret) break;
		ret = core_mem_write64(core, mem, temp_addr, temp_u64); // Write to memory
		if (ret) break;
		core->pc += 1;
		break;
	case op_storepop8:
		ret = core_stack_read64(core, mem, core->sp + 1, &temp_addr); // Read addr
		if (ret) break;
		ret = core_stack_read8(core, mem, core->sp, &temp_u8); // Read data
		if (ret) break;
		ret = core_mem_write8(core, mem, temp_addr, temp_u8); // Write to memory
		if (ret) break;
		core->sp += 1;
		core->pc += 1;
		break;
	case op_storepop16:
		ret = core_stack_read64(core, mem, core->sp + 2, &temp_addr); // Read addr
		if (ret) break;
		ret = core_stack_read16(core, mem, core->sp, &temp_u16); // Read data
		if (ret) break;
		ret = core_mem_write16(core, mem, temp_addr, temp_u16); // Write to memory
		if (ret) break;
		core->sp += 2;
		core->pc += 1;
		break;
	case op_storepop32:
		ret = core_stack_read64(core, mem, core->sp + 4, &temp_addr); // Read addr
		if (ret) break;
		ret = core_stack_read32(core, mem, core->sp, &temp_u32); // Read data
		if (ret) break;
		ret = core_mem_write32(core, mem, temp_addr, temp_u32); // Write to memory
		if (ret) break;
		core->sp += 4;
		core->pc += 1;
		break;
	case op_storepop64:
		ret = core_stack_read64(core, mem, core->sp + 8, &temp_addr); // Read addr
		if (ret) break;
		ret = core_stack_read64(core, mem, core->sp, &temp_u64); // Read data
		if (ret) break;
		ret = core_mem_write64(core, mem, temp_addr, temp_u64); // Write to memory
		if (ret) break;
		core->sp += 8;
		core->pc += 1;
		break;
	case op_storesfp8:
		ret = core_stack_read64(core, mem, core->sp + 1, &temp_u64); // Read offset
		if (ret) break;
		ret = core_stack_read8(core, mem, core->sp, &temp_u8); // Read data
		if (ret) break;
		ret = core_stack_write8(core, mem, core->sfp + (int64_t)temp_u64, temp_u8); // Write to stack
		if (ret) break;
		core->pc += 1;
		break;
	case op_storesfp16:
		ret = core_stack_read64(core, mem, core->sp + 2, &temp_u64); // Read offset
		if (ret) break;
		ret = core_stack_read16(core, mem, core->sp, &temp_u16); // Read data
		if (ret) break;
		ret = core_stack_write16(core, mem, core->sfp + (int64_t)temp_u64, temp_u16); // Write to stack
		if (ret) break;
		core->pc += 1;
		break;
	case op_storesfp32:
		ret = core_stack_read64(core, mem, core->sp + 4, &temp_u64); // Read offset
		if (ret) break;
		ret = core_stack_read32(core, mem, core->sp, &temp_u32); // Read data
		if (ret) break;
		ret = core_stack_write32(core, mem, core->sfp + (int64_t)temp_u64, temp_u32); // Write to stack
		if (ret) break;
		core->pc += 1;
		break;
	case op_storesfp64:
		ret = core_stack_read64(core, mem, core->sp + 8, &temp_u64); // Read offset
		if (ret) break;
		ret = core_stack_read64(core, mem, core->sp, &temp_u64b); // Read data
		if (ret) break;
		ret = core_stack_write64(core, mem, core->sfp + (int64_t)temp_u64, temp_u64b); // Write to stack
		if (ret) break;
		core->pc += 1;
		break;
	case op_storepopsfp8:
		ret = core_stack_read64(core, mem, core->sp + 1, &temp_u64); // Read offset
		if (ret) break;
		ret = core_stack_read8(core, mem, core->sp, &temp_u8); // Read data
		if (ret) break;
		ret = core_stack_write8(core, mem, core->sfp + (int64_t)temp_u64, temp_u8); // Write to stack
		if (ret) break;
		core->sp += 1;
		core->pc += 1;
		break;
	case op_storepopsfp16:
		ret = core_stack_read64(core, mem, core->sp + 2, &temp_u64); // Read offset
		if (ret) break;
		ret = core_stack_read16(core, mem, core->sp, &temp_u16); // Read data
		if (ret) break;
		ret = core_stack_write16(core, mem, core->sfp + (int64_t)temp_u64, temp_u16); // Write to stack
		if (ret) break;
		core->sp += 2;
		core->pc += 1;
		break;
	case op_storepopsfp32:
		ret = core_stack_read64(core, mem, core->sp + 4, &temp_u64); // Read offset
		if (ret) break;
		ret = core_stack_read32(core, mem, core->sp, &temp_u32); // Read data
		if (ret) break;
		ret = core_stack_write32(core, mem, core->sfp + (int64_t)temp_u64, temp_u32); // Write to stack
		if (ret) break;
		core->sp += 4;
		core->pc += 1;
		break;
	case op_storepopsfp64:
		ret = core_stack_read64(core, mem, core->sp + 8, &temp_u64); // Read offset
		if (ret) break;
		ret = core_stack_read64(core, mem, core->sp, &temp_u64b); // Read data
		if (ret) break;
		ret = core_stack_write64(core, mem, core->sfp + (int64_t)temp_u64, temp_u64b); // Write to stack
		if (ret) break;
		core->sp += 8;
		core->pc += 1;
		break;
	case op_storer8:
		ret = core_stack_read64(core, mem, core->sp, &temp_addr); // Read addr
		if (ret) break;
		ret = core_stack_read8(core, mem, core->sp + 8, &temp_u8); // Read data
		if (ret) break;
		ret = core_mem_write8(core, mem, temp_addr, temp_u8); // Write to memory
		if (ret) break;
		core->pc += 1;
		break;
	case op_storer16:
		ret = core_stack_read64(core, mem, core->sp, &temp_addr); // Read addr
		if (ret) break;
		ret = core_stack_read16(core, mem, core->sp + 8, &temp_u16); // Read data
		if (ret) break;
		ret = core_mem_write16(core, mem, temp_addr, temp_u16); // Write to memory
		if (ret) break;
		core->pc += 1;
		break;
	case op_storer32:
		ret = core_stack_read64(core, mem, core->sp, &temp_addr); // Read addr
		if (ret) break;
		ret = core_stack_read32(core, mem, core->sp + 8, &temp_u32); // Read data
		if (ret) break;
		ret = core_mem_write32(core, mem, temp_addr, temp_u32); // Write to memory
		if (ret) break;
		core->pc += 1;
		break;
	case op_storer64:
		ret = core_stack_read64(core, mem, core->sp, &temp_addr); // Read addr
		if (ret) break;
		ret = core_stack_read64(core, mem, core->sp + 8, &temp_u64); // Read data
		if (ret) break;
		ret = core_mem_write64(core, mem, temp_addr, temp_u64); // Write to memory
		if (ret) break;
		core->pc += 1;
		break;
	case op_storerpop8:
		ret = core_stack_read64(core, mem, core->sp, &temp_addr); // Read addr
		if (ret) break;
		ret = core_stack_read8(core, mem, core->sp + 8, &temp_u8); // Read data
		if (ret) break;
		ret = core_mem_write8(core, mem, temp_addr, temp_u8); // Write to memory
		if (ret) break;
		core->sp += 8;
		core->pc += 1;
		break;
	case op_storerpop16:
		ret = core_stack_read64(core, mem, core->sp, &temp_addr); // Read addr
		if (ret) break;
		ret = core_stack_read16(core, mem, core->sp + 8, &temp_u16); // Read data
		if (ret) break;
		ret = core_mem_write16(core, mem, temp_addr, temp_u16); // Write to memory
		if (ret) break;
		core->sp += 8;
		core->pc += 1;
		break;
	case op_storerpop32:
		ret = core_stack_read64(core, mem, core->sp, &temp_addr); // Read addr
		if (ret) break;
		ret = core_stack_read32(core, mem, core->sp + 8, &temp_u32); // Read data
		if (ret) break;
		ret = core_mem_write32(core, mem, temp_addr, temp_u32); // Write to memory
		if (ret) break;
		core->sp += 8;
		core->pc += 1;
		break;
	case op_storerpop64:
		ret = core_stack_read64(core, mem, core->sp, &temp_addr); // Read addr
		if (ret) break;
		ret = core_stack_read64(core, mem, core->sp + 8, &temp_u64); // Read data
		if (ret) break;
		ret = core_mem_write64(core, mem, temp_addr, temp_u64); // Write to memory
		if (ret) break;
		core->sp += 8;
		core->pc += 1;
		break;
	case op_storersfp8:
		ret = core_stack_read64(core, mem, core->sp, &temp_u64); // Read offset
		if (ret) break;
		ret = core_stack_read8(core, mem, core->sp + 8, &temp_u8); // Read data
		if (ret) break;
		ret = core_stack_write8(core, mem, core->sfp + (int64_t)temp_u64, temp_u8); // Write to stack
		if (ret) break;
		core->pc += 1;
		break;
	case op_storersfp16:
		ret = core_stack_read64(core, mem, core->sp, &temp_u64); // Read offset
		if (ret) break;
		ret = core_stack_read16(core, mem, core->sp + 8, &temp_u16); // Read data
		if (ret) break;
		ret = core_stack_write16(core, mem, core->sfp + (int64_t)temp_u64, temp_u16); // Write to stack
		if (ret) break;
		core->pc += 1;
		break;
	case op_storersfp32:
		ret = core_stack_read64(core, mem, core->sp, &temp_u64); // Read offset
		if (ret) break;
		ret = core_stack_read32(core, mem, core->sp + 8, &temp_u32); // Read data
		if (ret) break;
		ret = core_stack_write32(core, mem, core->sfp + (int64_t)temp_u64, temp_u32); // Write to stack
		if (ret) break;
		core->pc += 1;
		break;
	case op_storersfp64:
		ret = core_stack_read64(core, mem, core->sp, &temp_u64); // Read offset
		if (ret) break;
		ret = core_stack_read64(core, mem, core->sp + 8, &temp_u64b); // Read data
		if (ret) break;
		ret = core_stack_write64(core, mem, core->sfp + (int64_t)temp_u64, temp_u64b); // Write to stack
		if (ret) break;
		core->pc += 1;
		break;
	case op_storerpopsfp8:
		ret = core_stack_read64(core, mem, core->sp, &temp_u64); // Read offset
		if (ret) break;
		ret = core_stack_read8(core, mem, core->sp + 8, &temp_u8); // Read data
		if (ret) break;
		ret = core_stack_write8(core, mem, core->sfp + (int64_t)temp_u64, temp_u8); // Write to stack
		if (ret) break;
		core->sp += 8;
		core->pc += 1;
		break;
	case op_storerpopsfp16:
		ret = core_stack_read64(core, mem, core->sp, &temp_u64); // Read offset
		if (ret) break;
		ret = core_stack_read16(core, mem, core->sp + 8, &temp_u16); // Read data
		if (ret) break;
		ret = core_stack_write16(core, mem, core->sfp + (int64_t)temp_u64, temp_u16); // Write to stack
		if (ret) break;
		core->sp += 8;
		core->pc += 1;
		break;
	case op_storerpopsfp32:
		ret = core_stack_read64(core, mem, core->sp, &temp_u64); // Read offset
		if (ret) break;
		ret = core_stack_read32(core, mem, core->sp + 8, &temp_u32); // Read data
		if (ret) break;
		ret = core_stack_write32(core, mem, core->sfp + (int64_t)temp_u64, temp_u32); // Write to stack
		if (ret) break;
		core->sp += 8;
		core->pc += 1;
		break;
	case op_storerpopsfp64:
		ret = core_stack_read64(core, mem, core->sp, &temp_u64); // Read offset
		if (ret) break;
		ret = core_stack_read64(core, mem, core->sp + 8, &temp_u64b); // Read data
		if (ret) break;
		ret = core_stack_write64(core, mem, core->sfp + (int64_t)temp_u64, temp_u64b); // Write to stack
		if (ret) break;
		core->sp += 8;
		core->pc += 1;
		break;

	//
	// Special Operations
	//
	case op_setsbp:
		ret = core_mem_read64(core, mem, core->pc + 1, &temp_addr); // Read addr
		if (ret) break;
		core->sbp = temp_addr;
		core->pc += 9;
		break;
	case op_setsfp:
		ret = core_mem_read64(core, mem, core->pc + 1, &temp_addr); // Read addr
		if (ret) break;
		core->sfp = temp_addr;
		core->pc += 9;
		break;
	case op_setsp:
		ret = core_mem_read64(core, mem, core->pc + 1, &temp_addr); // Read addr
		if (ret) break;
		core->sp = temp_addr;
		core->pc += 9;
		break;
	case op_setslp:
		ret = core_mem_read64(core, mem, core->pc + 1, &temp_addr); // Read addr
		if (ret) break;
		core->slp = temp_addr;
		core->pc += 9;
		break;
	case op_halt:
		ret = core_mem_read8(core, mem, core->pc + 1, &temp_u8); // Read exit code imm
		if (ret) break;
		core_flush_stdout(core);
		ret = 256 + temp_u8;
		break;
	case op_ext:
		// @todo
		ret = -1;
		break;
	case op_nop:
		core->pc += 1;
		break;

	default:
		ret = -1;
		break;
	}

	if (ret > 0 && ret < 256) {
		// An interrupt occurred. Vector to interrupt handler.
		// @todo: Need a way to save and restore processor state like sfp.
		core->pc = BEGIN_INT_ADDR + 16 * ret;
	}

	return ret;
}
