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
	STACK_FRAME_METADATA_SIZE = 16,
};

void core_init(struct core *core)
{
	memset(core, 0, sizeof(struct core));
	core->cur = core->scbs; // Select context zero
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

// Read buflen random bytes into the buffer at buf.
// Returns zero on success, negative on failure.
static int core_get_random(void *buf, size_t buflen)
{
	int ret;

	static int inited;
	if (!inited) {
		// Initialize the random number generator with entropy
		enum { ENTROPY_SIZE = 128 };
		char entropy[ENTROPY_SIZE];
		ret = getentropy(entropy, ENTROPY_SIZE);
		if (ret != 0) {
			return ret;
		}
		initstate(0, entropy, ENTROPY_SIZE);
		inited = 1;
	}

	size_t i = 0;
	if (buflen >= sizeof(long)) {
		// Copy random data one long int at a time
		for (; i <= buflen - sizeof(long); i += sizeof(long)) {
			*(long*)((uint8_t*)buf + i) = random();
		}
	}
	if (i < buflen) {
		// Copy any remaining needed data a byte at a time
		long r = random();
		do {
			*((uint8_t*)buf + i++) = r;
			r >>= 8;
		} while (i < buflen);
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

static int core_frame_write8(struct core *core, struct mem *mem, uint64_t addr, uint8_t data)
{
	// Check stack frame bounds
	if (addr < core->cur->sfp || addr > core->cur->slp - 1) {
		return STINT_BAD_FRAME_ACCESS;
	}
	// Check stack bounds
	if (addr < core->cur->sbp) {
		return STINT_BAD_STACK_ACCESS;
	}

	return core_mem_write8(core, mem, addr, data);
}

static int core_stack_write8(struct core *core, struct mem *mem, uint64_t addr, uint8_t data)
{
	// Check stack bounds
	if (addr < core->cur->sbp || addr > core->cur->slp - 1) {
		return STINT_BAD_STACK_ACCESS;
	}

	return core_mem_write8(core, mem, addr, data);
}

static int core_mem_write16(struct core *core, struct mem *mem, uint64_t addr, uint16_t data)
{
	(void)core;

	// Check IO memory
	if (addr < END_IO_ADDR) {
		if (addr == IO_ASSERT_ADDR) {
			return data == 0 ? STINT_ASSERT_FAILURE : 0;
		}
		return STINT_BAD_IO_ACCESS; // No 16-bit IO write operations currently
	}

	return mem_write16(mem, addr, data);
}

static int core_frame_write16(struct core *core, struct mem *mem, uint64_t addr, uint16_t data)
{
	// Check stack frame bounds
	if (addr < core->cur->sfp || addr > core->cur->slp - 2) {
		return STINT_BAD_FRAME_ACCESS;
	}
	// Check stack bounds
	if (addr < core->cur->sbp) {
		return STINT_BAD_STACK_ACCESS;
	}

	return core_mem_write16(core, mem, addr, data);
}

static int core_stack_write16(struct core *core, struct mem *mem, uint64_t addr, uint16_t data)
{
	// Check stack bounds
	if (addr < core->cur->sbp || addr > core->cur->slp - 2) {
		return STINT_BAD_STACK_ACCESS;
	}

	return core_mem_write16(core, mem, addr, data);
}

static int core_mem_write32(struct core *core, struct mem *mem, uint64_t addr, uint32_t data)
{
	(void)core;

	// Check IO memory
	if (addr < END_IO_ADDR) {
		if (addr == IO_ASSERT_ADDR) {
			return data == 0 ? STINT_ASSERT_FAILURE : 0;
		}
		return STINT_BAD_IO_ACCESS; // No 32-bit IO write operations currently
	}

	return mem_write32(mem, addr, data);
}

static int core_frame_write32(struct core *core, struct mem *mem, uint64_t addr, uint32_t data)
{
	// Check stack frame bounds
	if (addr < core->cur->sfp || addr > core->cur->slp - 4) {
		return STINT_BAD_FRAME_ACCESS;
	}
	// Check stack bounds
	if (addr < core->cur->sbp) {
		return STINT_BAD_STACK_ACCESS;
	}

	return core_mem_write32(core, mem, addr, data);
}

static int core_stack_write32(struct core *core, struct mem *mem, uint64_t addr, uint32_t data)
{
	// Check stack bounds
	if (addr < core->cur->sbp || addr > core->cur->slp - 4) {
		return STINT_BAD_STACK_ACCESS;
	}

	return core_mem_write32(core, mem, addr, data);
}

static int core_mem_write64(struct core *core, struct mem *mem, uint64_t addr, uint64_t data)
{
	(void)core;

	// Check IO memory
	if (addr < END_IO_ADDR) {
		if (addr == IO_ASSERT_ADDR) {
			return data == 0 ? STINT_ASSERT_FAILURE : 0;
		}
		return STINT_BAD_IO_ACCESS; // No 64-bit IO write operations currently
	}

	return mem_write64(mem, addr, data);
}

static int core_frame_write64(struct core *core, struct mem *mem, uint64_t addr, uint64_t data)
{
	// Check stack frame bounds
	if (addr < core->cur->sfp || addr > core->cur->slp - 8) {
		return STINT_BAD_FRAME_ACCESS;
	}
	// Check stack bounds
	if (addr < core->cur->sbp) {
		return STINT_BAD_STACK_ACCESS;
	}

	return core_mem_write64(core, mem, addr, data);
}

static int core_stack_write64(struct core *core, struct mem *mem, uint64_t addr, uint64_t data)
{
	// Check stack bounds
	if (addr < core->cur->sbp || addr > core->cur->slp - 8) {
		return STINT_BAD_STACK_ACCESS;
	}

	return core_mem_write64(core, mem, addr, data);
}

static int core_mem_read8(struct core *core, struct mem *mem, uint64_t addr, uint8_t *data)
{
	// Check IO memory
	if (addr < END_IO_ADDR) {
		if (addr == IO_STDIN_ADDR) {
			return core_read_stdin(core, data);
		}
		if (addr == IO_URAND_ADDR) {
			return core_get_random(data, sizeof(*data));
		}
		return STINT_BAD_IO_ACCESS; // No 8-bit IO read operations currently
	}

	return mem_read8(mem, addr, data);
}

static int core_frame_read8(struct core *core, struct mem *mem, uint64_t addr, uint8_t *data)
{
	// Check stack frame bounds
	if (addr < core->cur->sfp || addr > core->cur->slp - 1) {
		return STINT_BAD_FRAME_ACCESS;
	}
	// Check stack bounds
	if (addr < core->cur->sbp) {
		return STINT_BAD_STACK_ACCESS;
	}

	return core_mem_read8(core, mem, addr, data);
}

static int core_stack_read8(struct core *core, struct mem *mem, uint64_t addr, uint8_t *data)
{
	// Check stack bounds
	if (addr < core->cur->sbp || addr > core->cur->slp - 1) {
		return STINT_BAD_STACK_ACCESS;
	}

	return core_mem_read8(core, mem, addr, data);
}

static int core_mem_read16(struct core *core, struct mem *mem, uint64_t addr, uint16_t *data)
{
	(void)core;

	// Check IO memory
	if (addr < END_IO_ADDR) {
		if (addr == IO_URAND_ADDR) {
			return core_get_random(data, sizeof(*data));
		}
		return STINT_BAD_IO_ACCESS; // No 16-bit IO read operations currently
	}

	return mem_read16(mem, addr, data);
}

static int core_frame_read16(struct core *core, struct mem *mem, uint64_t addr, uint16_t *data)
{
	// Check stack frame bounds
	if (addr < core->cur->sfp || addr > core->cur->slp - 2) {
		return STINT_BAD_FRAME_ACCESS;
	}
	// Check stack bounds
	if (addr < core->cur->sbp) {
		return STINT_BAD_STACK_ACCESS;
	}

	return core_mem_read16(core, mem, addr, data);
}

static int core_stack_read16(struct core *core, struct mem *mem, uint64_t addr, uint16_t *data)
{
	// Check stack bounds
	if (addr < core->cur->sbp || addr > core->cur->slp - 2) {
		return STINT_BAD_STACK_ACCESS;
	}

	return core_mem_read16(core, mem, addr, data);
}

static int core_mem_read32(struct core *core, struct mem *mem, uint64_t addr, uint32_t *data)
{
	(void)core;

	// Check IO memory
	if (addr < END_IO_ADDR) {
		if (addr == IO_URAND_ADDR) {
			return core_get_random(data, sizeof(*data));
		}
		return STINT_BAD_IO_ACCESS;
	}

	return mem_read32(mem, addr, data);
}

static int core_frame_read32(struct core *core, struct mem *mem, uint64_t addr, uint32_t *data)
{
	// Check stack frame bounds
	if (addr < core->cur->sfp || addr > core->cur->slp - 4) {
		return STINT_BAD_FRAME_ACCESS;
	}
	// Check stack bounds
	if (addr < core->cur->sbp) {
		return STINT_BAD_STACK_ACCESS;
	}

	return core_mem_read32(core, mem, addr, data);
}

static int core_stack_read32(struct core *core, struct mem *mem, uint64_t addr, uint32_t *data)
{
	// Check stack bounds
	if (addr < core->cur->sbp || addr > core->cur->slp - 4) {
		return STINT_BAD_STACK_ACCESS;
	}

	return core_mem_read32(core, mem, addr, data);
}

static int core_mem_read64(struct core *core, struct mem *mem, uint64_t addr, uint64_t *data)
{
	(void)core;

	// Check IO memory
	if (addr < END_IO_ADDR) {
		if (addr == IO_URAND_ADDR) {
			return core_get_random(data, sizeof(*data));
		}
		return STINT_BAD_IO_ACCESS; // No 64-bit IO read operations currently
	}

	return mem_read64(mem, addr, data);
}

static int core_frame_read64(struct core *core, struct mem *mem, uint64_t addr, uint64_t *data)
{
	// Check stack frame bounds
	if (addr < core->cur->sfp || addr > core->cur->slp - 8) {
		return STINT_BAD_FRAME_ACCESS;
	}
	// Check stack bounds
	if (addr < core->cur->sbp) {
		return STINT_BAD_STACK_ACCESS;
	}

	return core_mem_read64(core, mem, addr, data);
}

static int core_stack_read64(struct core *core, struct mem *mem, uint64_t addr, uint64_t *data)
{
	// Check stack bounds
	if (addr < core->cur->sbp || addr > core->cur->slp - 8) {
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
	uint64_t temp_u64, temp_u64b;

	//
	// Execute instruction on core and memory
	//
	if (ret == 0) switch (opcode) {
	//
	// Invalid instruction
	//
	case op_invalid:
		core->pc += 1;
		ret = STINT_INVALID_INST;
		break;

	//
	// Push immediate operations
	//
	case op_push8as8:
		core->pc += 2;
		ret = core_mem_read8(core, mem, core->pc - 1, &temp_u8); // Read imm
		if (ret) break;
		ret = core_frame_write8(core, mem, core->cur->sp, temp_u8); // Write to stack
		if (ret) break;
		core->cur->sp += 1;
		break;
	case op_push8asu16:
		core->pc += 2;
		ret = core_mem_read8(core, mem, core->pc - 1, &temp_u8); // Read imm
		if (ret) break;
		ret = core_frame_write16(core, mem, core->cur->sp, temp_u8); // Write to stack
		if (ret) break;
		core->cur->sp += 2;
		break;
	case op_push8asu32:
		core->pc += 2;
		ret = core_mem_read8(core, mem, core->pc - 1, &temp_u8); // Read imm
		if (ret) break;
		ret = core_frame_write32(core, mem, core->cur->sp, temp_u8); // Write to stack
		if (ret) break;
		core->cur->sp += 4;
		break;
	case op_push8asu64:
		core->pc += 2;
		ret = core_mem_read8(core, mem, core->pc - 1, &temp_u8); // Read imm
		if (ret) break;
		ret = core_frame_write64(core, mem, core->cur->sp, temp_u8); // Write to stack
		if (ret) break;
		core->cur->sp += 8;
		break;
	case op_push8asi16:
		core->pc += 2;
		ret = core_mem_read8(core, mem, core->pc - 1, &temp_u8); // Read imm
		if (ret) break;
		ret = core_frame_write16(core, mem, core->cur->sp, (int8_t)temp_u8); // Write to stack
		if (ret) break;
		core->cur->sp += 2;
		break;
	case op_push8asi32:
		core->pc += 2;
		ret = core_mem_read8(core, mem, core->pc - 1, &temp_u8); // Read imm
		if (ret) break;
		ret = core_frame_write32(core, mem, core->cur->sp, (int8_t)temp_u8); // Write to stack
		if (ret) break;
		core->cur->sp += 4;
		break;
	case op_push8asi64:
		core->pc += 2;
		ret = core_mem_read8(core, mem, core->pc - 1, &temp_u8); // Read imm
		if (ret) break;
		ret = core_frame_write64(core, mem, core->cur->sp, (int8_t)temp_u8); // Write to stack
		if (ret) break;
		core->cur->sp += 8;
		break;
	case op_push16as16:
		core->pc += 3;
		ret = core_mem_read16(core, mem, core->pc - 2, &temp_u16); // Read imm
		if (ret) break;
		ret = core_frame_write16(core, mem, core->cur->sp, temp_u16); // Write to stack
		if (ret) break;
		core->cur->sp += 2;
		break;
	case op_push16asu32:
		core->pc += 3;
		ret = core_mem_read16(core, mem, core->pc - 2, &temp_u16); // Read imm
		if (ret) break;
		ret = core_frame_write32(core, mem, core->cur->sp, temp_u16); // Write to stack
		if (ret) break;
		core->cur->sp += 4;
		break;
	case op_push16asu64:
		core->pc += 3;
		ret = core_mem_read16(core, mem, core->pc - 2, &temp_u16); // Read imm
		if (ret) break;
		ret = core_frame_write64(core, mem, core->cur->sp, temp_u16); // Write to stack
		if (ret) break;
		core->cur->sp += 8;
		break;
	case op_push16asi32:
		core->pc += 3;
		ret = core_mem_read16(core, mem, core->pc - 2, &temp_u16); // Read imm
		if (ret) break;
		ret = core_frame_write32(core, mem, core->cur->sp, (int16_t)temp_u16); // Write to stack
		if (ret) break;
		core->cur->sp += 4;
		break;
	case op_push16asi64:
		core->pc += 3;
		ret = core_mem_read16(core, mem, core->pc - 2, &temp_u16); // Read imm
		if (ret) break;
		ret = core_frame_write64(core, mem, core->cur->sp, (int16_t)temp_u16); // Write to stack
		if (ret) break;
		core->cur->sp += 8;
		break;
	case op_push32as32:
		core->pc += 5;
		ret = core_mem_read32(core, mem, core->pc - 4, &temp_u32); // Read imm
		if (ret) break;
		ret = core_frame_write32(core, mem, core->cur->sp, temp_u32); // Write to stack
		if (ret) break;
		core->cur->sp += 4;
		break;
	case op_push32asu64:
		core->pc += 5;
		ret = core_mem_read32(core, mem, core->pc - 4, &temp_u32); // Read imm
		if (ret) break;
		ret = core_frame_write64(core, mem, core->cur->sp, temp_u32); // Write to stack
		if (ret) break;
		core->cur->sp += 8;
		break;
	case op_push32asi64:
		core->pc += 5;
		ret = core_mem_read32(core, mem, core->pc - 4, &temp_u32); // Read imm
		if (ret) break;
		ret = core_frame_write64(core, mem, core->cur->sp, (int32_t)temp_u32); // Write to stack
		if (ret) break;
		core->cur->sp += 8;
		break;
	case op_push64as64:
		core->pc += 9;
		ret = core_mem_read64(core, mem, core->pc - 8, &temp_u64); // Read imm
		if (ret) break;
		ret = core_frame_write64(core, mem, core->cur->sp, temp_u64); // Write to stack
		if (ret) break;
		core->cur->sp += 8;
		break;

	//
	// Pop operations
	//
	case op_pop8:
		core->pc += 1;
		core->cur->sp -= 1;
		break;
	case op_pop16:
		core->pc += 1;
		core->cur->sp -= 2;
		break;
	case op_pop32:
		core->pc += 1;
		core->cur->sp -= 4;
		break;
	case op_pop64:
		core->pc += 1;
		core->cur->sp -= 8;
		break;
	case op_popn:
		core->pc += 1;
		ret = core_frame_read64(core, mem, core->cur->sp - 8, &temp_u64);
		if (ret) break;
		core->cur->sp += -(int64_t)temp_u64 - 8;
		break;

	//
	// Duplication operations
	//
	case op_dup8:
		core->pc += 1;
		ret = core_frame_read8(core, mem, core->cur->sp - 1, &temp_u8);
		if (ret) break;
		ret = core_frame_write8(core, mem, core->cur->sp, temp_u8);
		if (ret) break;
		core->cur->sp += 1;
		break;
	case op_dup16:
		core->pc += 1;
		ret = core_frame_read16(core, mem, core->cur->sp - 2, &temp_u16);
		if (ret) break;
		ret = core_frame_write16(core, mem, core->cur->sp, temp_u16);
		if (ret) break;
		core->cur->sp += 2;
		break;
	case op_dup32:
		core->pc += 1;
		ret = core_frame_read32(core, mem, core->cur->sp - 4, &temp_u32);
		if (ret) break;
		ret = core_frame_write32(core, mem, core->cur->sp, temp_u32);
		if (ret) break;
		core->cur->sp += 4;
		break;
	case op_dup64:
		core->pc += 1;
		ret = core_frame_read64(core, mem, core->cur->sp - 8, &temp_u64);
		if (ret) break;
		ret = core_frame_write64(core, mem, core->cur->sp, temp_u64);
		if (ret) break;
		core->cur->sp += 8;
		break;

	//
	// Setting operations
	//
	case op_set8:
		core->pc += 1;
		ret = core_frame_read8(core, mem, core->cur->sp - 1, &temp_u8);
		if (ret) break;
		ret = core_frame_write8(core, mem, core->cur->sp - 2, temp_u8);
		if (ret) break;
		core->cur->sp -= 1;
		break;
	case op_set16:
		core->pc += 1;
		ret = core_frame_read16(core, mem, core->cur->sp - 2, &temp_u16);
		if (ret) break;
		ret = core_frame_write16(core, mem, core->cur->sp - 4, temp_u16);
		if (ret) break;
		core->cur->sp -= 2;
		break;
	case op_set32:
		core->pc += 1;
		ret = core_frame_read32(core, mem, core->cur->sp - 4, &temp_u32);
		if (ret) break;
		ret = core_frame_write32(core, mem, core->cur->sp - 8, temp_u32);
		if (ret) break;
		core->cur->sp -= 4;
		break;
	case op_set64:
		core->pc += 1;
		ret = core_frame_read64(core, mem, core->cur->sp - 8, &temp_u64);
		if (ret) break;
		ret = core_frame_write64(core, mem, core->cur->sp - 16, temp_u64);
		if (ret) break;
		core->cur->sp -= 8;
		break;

	//
	// Promotion operations
	//
	case op_prom8u16:
		core->pc += 1;
		ret = core_frame_write8(core, mem, core->cur->sp, 0);
		if (ret) break;
		core->cur->sp += 1;
		break;
	case op_prom8u32:
		core->pc += 1;
		ret = core_frame_write8(core, mem, core->cur->sp, 0);
		if (ret) break;
		ret = core_frame_write16(core, mem, core->cur->sp + 1, 0);
		if (ret) break;
		core->cur->sp += 3;
		break;
	case op_prom8u64:
		core->pc += 1;
		ret = core_frame_write32(core, mem, core->cur->sp, 0);
		if (ret) break;
		ret = core_frame_write32(core, mem, core->cur->sp + 3, 0);
		if (ret) break;
		core->cur->sp += 7;
		break;
	case op_prom8i16:
		core->pc += 1;
		ret = core_frame_read8(core, mem, core->cur->sp - 1, &temp_u8);
		if (ret) break;
		ret = core_frame_write16(core, mem, core->cur->sp - 1, (int8_t)temp_u8);
		if (ret) break;
		core->cur->sp += 1;
		break;
	case op_prom8i32:
		core->pc += 1;
		ret = core_frame_read8(core, mem, core->cur->sp - 1, &temp_u8);
		if (ret) break;
		ret = core_frame_write32(core, mem, core->cur->sp - 1, (int8_t)temp_u8);
		if (ret) break;
		core->cur->sp += 3;
		break;
	case op_prom8i64:
		core->pc += 1;
		ret = core_frame_read8(core, mem, core->cur->sp - 1, &temp_u8);
		if (ret) break;
		ret = core_frame_write64(core, mem, core->cur->sp - 1, (int8_t)temp_u8);
		if (ret) break;
		core->cur->sp += 7;
		break;
	case op_prom16u32:
		core->pc += 1;
		ret = core_frame_write16(core, mem, core->cur->sp, 0);
		if (ret) break;
		core->cur->sp += 2;
		break;
	case op_prom16u64:
		core->pc += 1;
		ret = core_frame_write16(core, mem, core->cur->sp, 0);
		if (ret) break;
		ret = core_frame_write32(core, mem, core->cur->sp + 2, 0);
		if (ret) break;
		core->cur->sp += 6;
		break;
	case op_prom16i32:
		core->pc += 1;
		ret = core_frame_read16(core, mem, core->cur->sp - 2, &temp_u16);
		if (ret) break;
		ret = core_frame_write32(core, mem, core->cur->sp - 2, (int16_t)temp_u16);
		if (ret) break;
		core->cur->sp += 2;
		break;
	case op_prom16i64:
		core->pc += 1;
		ret = core_frame_read16(core, mem, core->cur->sp - 2, &temp_u16);
		if (ret) break;
		ret = core_frame_write64(core, mem, core->cur->sp - 2, (int16_t)temp_u16);
		if (ret) break;
		core->cur->sp += 6;
		break;
	case op_prom32u64:
		core->pc += 1;
		ret = core_frame_write32(core, mem, core->cur->sp, 0);
		if (ret) break;
		core->cur->sp += 4;
		break;
	case op_prom32i64:
		core->pc += 1;
		ret = core_frame_read32(core, mem, core->cur->sp - 4, &temp_u32);
		if (ret) break;
		ret = core_frame_write64(core, mem, core->cur->sp - 4, (int32_t)temp_u32);
		if (ret) break;
		core->cur->sp += 4;
		break;

	//
	// Demotion operations
	//
	case op_dem64to16:
		core->pc += 1;
		core->cur->sp -= 6;
		break;
	case op_dem64to8:
		core->pc += 1;
		core->cur->sp -= 7;
		break;
	case op_dem32to8:
		core->pc += 1;
		core->cur->sp -= 3;
		break;

	//
	// Integer arithmetic operations
	//
	case op_add8:
		core->pc += 1;
		ret = core_frame_read8(core, mem, core->cur->sp - 1, &temp_u8b); // Read operand
		if (ret) break;
		ret = core_frame_read8(core, mem, core->cur->sp - 2, &temp_u8); // Read operand
		if (ret) break;
		ret = core_frame_write8(core, mem, core->cur->sp - 2, temp_u8 + temp_u8b); // Write sum
		if (ret) break;
		core->cur->sp -= 1;
		break;
	case op_add16:
		core->pc += 1;
		ret = core_frame_read16(core, mem, core->cur->sp - 2, &temp_u16b); // Read operand
		if (ret) break;
		ret = core_frame_read16(core, mem, core->cur->sp - 4, &temp_u16); // Read operand
		if (ret) break;
		ret = core_frame_write16(core, mem, core->cur->sp - 4, temp_u16 + temp_u16b); // Write sum
		if (ret) break;
		core->cur->sp -= 2;
		break;
	case op_add32:
		core->pc += 1;
		ret = core_frame_read32(core, mem, core->cur->sp - 4, &temp_u32b); // Read operand
		if (ret) break;
		ret = core_frame_read32(core, mem, core->cur->sp - 8, &temp_u32); // Read operand
		if (ret) break;
		ret = core_frame_write32(core, mem, core->cur->sp - 8, temp_u32 + temp_u32b); // Write sum
		if (ret) break;
		core->cur->sp -= 4;
		break;
	case op_add64:
		core->pc += 1;
		ret = core_frame_read64(core, mem, core->cur->sp - 8, &temp_u64b); // Read operand
		if (ret) break;
		ret = core_frame_read64(core, mem, core->cur->sp - 16, &temp_u64); // Read operand
		if (ret) break;
		ret = core_frame_write64(core, mem, core->cur->sp - 16, temp_u64 + temp_u64b); // Write sum
		if (ret) break;
		core->cur->sp -= 8;
		break;
	case op_sub8:
		core->pc += 1;
		ret = core_frame_read8(core, mem, core->cur->sp - 1, &temp_u8b); // Read operand
		if (ret) break;
		ret = core_frame_read8(core, mem, core->cur->sp - 2, &temp_u8); // Read operand
		if (ret) break;
		ret = core_frame_write8(core, mem, core->cur->sp - 2, temp_u8 - temp_u8b); // Write difference
		if (ret) break;
		core->cur->sp -= 1;
		break;
	case op_sub16:
		core->pc += 1;
		ret = core_frame_read16(core, mem, core->cur->sp -2, &temp_u16b); // Read operand
		if (ret) break;
		ret = core_frame_read16(core, mem, core->cur->sp - 4, &temp_u16); // Read operand
		if (ret) break;
		ret = core_frame_write16(core, mem, core->cur->sp - 4, temp_u16 - temp_u16b); // Write difference
		if (ret) break;
		core->cur->sp -= 2;
		break;
	case op_sub32:
		core->pc += 1;
		ret = core_frame_read32(core, mem, core->cur->sp - 4, &temp_u32b); // Read operand
		if (ret) break;
		ret = core_frame_read32(core, mem, core->cur->sp - 8, &temp_u32); // Read operand
		if (ret) break;
		ret = core_frame_write32(core, mem, core->cur->sp - 8, temp_u32 - temp_u32b); // Write difference
		if (ret) break;
		core->cur->sp -= 4;
		break;
	case op_sub64:
		core->pc += 1;
		ret = core_frame_read64(core, mem, core->cur->sp - 8, &temp_u64b); // Read operand
		if (ret) break;
		ret = core_frame_read64(core, mem, core->cur->sp - 16, &temp_u64); // Read operand
		if (ret) break;
		ret = core_frame_write64(core, mem, core->cur->sp - 16, temp_u64 - temp_u64b); // Write difference
		if (ret) break;
		core->cur->sp -= 8;
		break;
	case op_subr8:
		core->pc += 1;
		ret = core_frame_read8(core, mem, core->cur->sp - 1, &temp_u8b); // Read operand
		if (ret) break;
		ret = core_frame_read8(core, mem, core->cur->sp - 2, &temp_u8); // Read operand
		if (ret) break;
		ret = core_frame_write8(core, mem, core->cur->sp - 2, temp_u8b - temp_u8); // Write difference
		if (ret) break;
		core->cur->sp -= 1;
		break;
	case op_subr16:
		core->pc += 1;
		ret = core_frame_read16(core, mem, core->cur->sp - 2, &temp_u16b); // Read operand
		if (ret) break;
		ret = core_frame_read16(core, mem, core->cur->sp - 4, &temp_u16); // Read operand
		if (ret) break;
		ret = core_frame_write16(core, mem, core->cur->sp - 4, temp_u16b - temp_u16); // Write difference
		if (ret) break;
		core->cur->sp -= 2;
		break;
	case op_subr32:
		core->pc += 1;
		ret = core_frame_read32(core, mem, core->cur->sp - 4, &temp_u32b); // Read operand
		if (ret) break;
		ret = core_frame_read32(core, mem, core->cur->sp - 8, &temp_u32); // Read operand
		if (ret) break;
		ret = core_frame_write32(core, mem, core->cur->sp - 8, temp_u32b - temp_u32); // Write difference
		if (ret) break;
		core->cur->sp -= 4;
		break;
	case op_subr64:
		core->pc += 1;
		ret = core_frame_read64(core, mem, core->cur->sp - 8, &temp_u64b); // Read operand
		if (ret) break;
		ret = core_frame_read64(core, mem, core->cur->sp - 16, &temp_u64); // Read operand
		if (ret) break;
		ret = core_frame_write64(core, mem, core->cur->sp - 16, temp_u64b - temp_u64); // Write difference
		if (ret) break;
		core->cur->sp -= 8;
		break;
	case op_mul8:
		core->pc += 1;
		ret = core_frame_read8(core, mem, core->cur->sp - 1, &temp_u8b); // Read operand
		if (ret) break;
		ret = core_frame_read8(core, mem, core->cur->sp - 2, &temp_u8); // Read operand
		if (ret) break;
		ret = core_frame_write8(core, mem, core->cur->sp - 2, temp_u8 * temp_u8b); // Write product
		if (ret) break;
		core->cur->sp -= 1;
		break;
	case op_mul16:
		core->pc += 1;
		ret = core_frame_read16(core, mem, core->cur->sp - 2, &temp_u16b); // Read operand
		if (ret) break;
		ret = core_frame_read16(core, mem, core->cur->sp - 4, &temp_u16); // Read operand
		if (ret) break;
		ret = core_frame_write16(core, mem, core->cur->sp - 4, temp_u16 * temp_u16b); // Write product
		if (ret) break;
		core->cur->sp -= 2;
		break;
	case op_mul32:
		core->pc += 1;
		ret = core_frame_read32(core, mem, core->cur->sp - 4, &temp_u32b); // Read operand
		if (ret) break;
		ret = core_frame_read32(core, mem, core->cur->sp - 8, &temp_u32); // Read operand
		if (ret) break;
		ret = core_frame_write32(core, mem, core->cur->sp - 8, temp_u32 * temp_u32b); // Write product
		if (ret) break;
		core->cur->sp -= 4;
		break;
	case op_mul64:
		core->pc += 1;
		ret = core_frame_read64(core, mem, core->cur->sp - 8, &temp_u64b); // Read operand
		if (ret) break;
		ret = core_frame_read64(core, mem, core->cur->sp - 16, &temp_u64); // Read operand
		if (ret) break;
		ret = core_frame_write64(core, mem, core->cur->sp - 16, temp_u64 * temp_u64b); // Write product
		if (ret) break;
		core->cur->sp -= 8;
		break;
	case op_divu8:
		core->pc += 1;
		ret = core_frame_read8(core, mem, core->cur->sp - 1, &temp_u8b); // Read operand
		if (ret) break;
		ret = core_frame_read8(core, mem, core->cur->sp - 2, &temp_u8); // Read operand
		if (ret) break;
		if (temp_u8b == 0) {
			ret = STINT_DIV_BY_ZERO;
			break;
		}
		ret = core_frame_write8(core, mem, core->cur->sp - 2, temp_u8 / temp_u8b); // Write quotient
		if (ret) break;
		core->cur->sp -= 1;
		break;
	case op_divu16:
		core->pc += 1;
		ret = core_frame_read16(core, mem, core->cur->sp - 2, &temp_u16b); // Read operand
		if (ret) break;
		ret = core_frame_read16(core, mem, core->cur->sp - 4, &temp_u16); // Read operand
		if (ret) break;
		if (temp_u16b == 0) {
			ret = STINT_DIV_BY_ZERO;
			break;
		}
		ret = core_frame_write16(core, mem, core->cur->sp - 4, temp_u16 / temp_u16b); // Write quotient
		if (ret) break;
		core->cur->sp -= 2;
		break;
	case op_divu32:
		core->pc += 1;
		ret = core_frame_read32(core, mem, core->cur->sp - 4, &temp_u32b); // Read operand
		if (ret) break;
		ret = core_frame_read32(core, mem, core->cur->sp - 8, &temp_u32); // Read operand
		if (ret) break;
		if (temp_u32b == 0) {
			ret = STINT_DIV_BY_ZERO;
			break;
		}
		ret = core_frame_write32(core, mem, core->cur->sp - 8, temp_u32 / temp_u32b); // Write quotient
		if (ret) break;
		core->cur->sp -= 4;
		break;
	case op_divu64:
		core->pc += 1;
		ret = core_frame_read64(core, mem, core->cur->sp - 8, &temp_u64b); // Read operand
		if (ret) break;
		ret = core_frame_read64(core, mem, core->cur->sp - 16, &temp_u64); // Read operand
		if (ret) break;
		if (temp_u64b == 0) {
			ret = STINT_DIV_BY_ZERO;
			break;
		}
		ret = core_frame_write64(core, mem, core->cur->sp - 16, temp_u64 / temp_u64b); // Write quotient
		if (ret) break;
		core->cur->sp -= 8;
		break;
	case op_divru8:
		core->pc += 1;
		ret = core_frame_read8(core, mem, core->cur->sp - 1, &temp_u8b); // Read operand
		if (ret) break;
		ret = core_frame_read8(core, mem, core->cur->sp - 2, &temp_u8); // Read operand
		if (ret) break;
		if (temp_u8 == 0) {
			ret = STINT_DIV_BY_ZERO;
			break;
		}
		ret = core_frame_write8(core, mem, core->cur->sp - 2, temp_u8b / temp_u8); // Write quotient
		if (ret) break;
		core->cur->sp -= 1;
		break;
	case op_divru16:
		core->pc += 1;
		ret = core_frame_read16(core, mem, core->cur->sp - 2, &temp_u16b); // Read operand
		if (ret) break;
		ret = core_frame_read16(core, mem, core->cur->sp - 4, &temp_u16); // Read operand
		if (ret) break;
		if (temp_u16 == 0) {
			ret = STINT_DIV_BY_ZERO;
			break;
		}
		ret = core_frame_write16(core, mem, core->cur->sp - 4, temp_u16b / temp_u16); // Write quotient
		if (ret) break;
		core->cur->sp -= 2;
		break;
	case op_divru32:
		core->pc += 1;
		ret = core_frame_read32(core, mem, core->cur->sp - 4, &temp_u32b); // Read operand
		if (ret) break;
		ret = core_frame_read32(core, mem, core->cur->sp - 8, &temp_u32); // Read operand
		if (ret) break;
		if (temp_u32 == 0) {
			ret = STINT_DIV_BY_ZERO;
			break;
		}
		ret = core_frame_write32(core, mem, core->cur->sp - 8, temp_u32b / temp_u32); // Write quotient
		if (ret) break;
		core->cur->sp -= 4;
		break;
	case op_divru64:
		core->pc += 1;
		ret = core_frame_read64(core, mem, core->cur->sp - 8, &temp_u64b); // Read operand
		if (ret) break;
		ret = core_frame_read64(core, mem, core->cur->sp - 16, &temp_u64); // Read operand
		if (ret) break;
		if (temp_u64 == 0) {
			ret = STINT_DIV_BY_ZERO;
			break;
		}
		ret = core_frame_write64(core, mem, core->cur->sp - 16, temp_u64b / temp_u64); // Write quotient
		if (ret) break;
		core->cur->sp -= 8;
		break;
	case op_divi8:
		core->pc += 1;
		ret = core_frame_read8(core, mem, core->cur->sp - 1, &temp_u8b); // Read operand
		if (ret) break;
		ret = core_frame_read8(core, mem, core->cur->sp - 2, &temp_u8); // Read operand
		if (ret) break;
		if (temp_u8b == 0) {
			ret = STINT_DIV_BY_ZERO;
			break;
		}
		ret = core_frame_write8(core, mem, core->cur->sp - 2, (int8_t)temp_u8 / (int8_t)temp_u8b); // Write quotient
		if (ret) break;
		core->cur->sp -= 1;
		break;
	case op_divi16:
		core->pc += 1;
		ret = core_frame_read16(core, mem, core->cur->sp - 2, &temp_u16b); // Read operand
		if (ret) break;
		ret = core_frame_read16(core, mem, core->cur->sp - 4, &temp_u16); // Read operand
		if (ret) break;
		if (temp_u16b == 0) {
			ret = STINT_DIV_BY_ZERO;
			break;
		}
		ret = core_frame_write16(core, mem, core->cur->sp - 4, (int16_t)temp_u16 / (int16_t)temp_u16b); // Write quotient
		if (ret) break;
		core->cur->sp -= 2;
		break;
	case op_divi32:
		core->pc += 1;
		ret = core_frame_read32(core, mem, core->cur->sp - 4, &temp_u32b); // Read operand
		if (ret) break;
		ret = core_frame_read32(core, mem, core->cur->sp - 8, &temp_u32); // Read operand
		if (ret) break;
		if (temp_u32b == 0) {
			ret = STINT_DIV_BY_ZERO;
			break;
		}
		ret = core_frame_write32(core, mem, core->cur->sp - 8, (int32_t)temp_u32 / (int32_t)temp_u32b); // Write quotient
		if (ret) break;
		core->cur->sp -= 4;
		break;
	case op_divi64:
		core->pc += 1;
		ret = core_frame_read64(core, mem, core->cur->sp - 8, &temp_u64b); // Read operand
		if (ret) break;
		ret = core_frame_read64(core, mem, core->cur->sp - 16, &temp_u64); // Read operand
		if (ret) break;
		if (temp_u64b == 0) {
			ret = STINT_DIV_BY_ZERO;
			break;
		}
		ret = core_frame_write64(core, mem, core->cur->sp - 16, (int64_t)temp_u64 / (int64_t)temp_u64b); // Write quotient
		if (ret) break;
		core->cur->sp -= 8;
		break;
	case op_divri8:
		core->pc += 1;
		ret = core_frame_read8(core, mem, core->cur->sp - 1, &temp_u8b); // Read operand
		if (ret) break;
		ret = core_frame_read8(core, mem, core->cur->sp - 2, &temp_u8); // Read operand
		if (ret) break;
		if (temp_u8 == 0) {
			ret = STINT_DIV_BY_ZERO;
			break;
		}
		ret = core_frame_write8(core, mem, core->cur->sp - 2, (int8_t)temp_u8b / (int8_t)temp_u8); // Write quotient
		if (ret) break;
		core->cur->sp -= 1;
		break;
	case op_divri16:
		core->pc += 1;
		ret = core_frame_read16(core, mem, core->cur->sp - 2, &temp_u16b); // Read operand
		if (ret) break;
		ret = core_frame_read16(core, mem, core->cur->sp - 4, &temp_u16); // Read operand
		if (ret) break;
		if (temp_u16 == 0) {
			ret = STINT_DIV_BY_ZERO;
			break;
		}
		ret = core_frame_write16(core, mem, core->cur->sp - 4, (int16_t)temp_u16b / (int16_t)temp_u16); // Write quotient
		if (ret) break;
		core->cur->sp -= 2;
		break;
	case op_divri32:
		core->pc += 1;
		ret = core_frame_read32(core, mem, core->cur->sp - 4, &temp_u32b); // Read operand
		if (ret) break;
		ret = core_frame_read32(core, mem, core->cur->sp - 8, &temp_u32); // Read operand
		if (ret) break;
		if (temp_u32 == 0) {
			ret = STINT_DIV_BY_ZERO;
			break;
		}
		ret = core_frame_write32(core, mem, core->cur->sp - 8, (int32_t)temp_u32b / (int32_t)temp_u32); // Write quotient
		if (ret) break;
		core->cur->sp -= 4;
		break;
	case op_divri64:
		core->pc += 1;
		ret = core_frame_read64(core, mem, core->cur->sp - 8, &temp_u64b); // Read operand
		if (ret) break;
		ret = core_frame_read64(core, mem, core->cur->sp - 16, &temp_u64); // Read operand
		if (ret) break;
		if (temp_u64 == 0) {
			ret = STINT_DIV_BY_ZERO;
			break;
		}
		ret = core_frame_write64(core, mem, core->cur->sp - 16, (int64_t)temp_u64b / (int64_t)temp_u64); // Write quotient
		if (ret) break;
		core->cur->sp -= 8;
		break;
	case op_modu8:
		core->pc += 1;
		ret = core_frame_read8(core, mem, core->cur->sp - 1, &temp_u8b); // Read operand
		if (ret) break;
		ret = core_frame_read8(core, mem, core->cur->sp - 2, &temp_u8); // Read operand
		if (ret) break;
		if (temp_u8b == 0) {
			ret = STINT_DIV_BY_ZERO;
			break;
		}
		ret = core_frame_write8(core, mem, core->cur->sp - 2, temp_u8 % temp_u8b); // Write remainder
		if (ret) break;
		core->cur->sp -= 1;
		break;
	case op_modu16:
		core->pc += 1;
		ret = core_frame_read16(core, mem, core->cur->sp - 2, &temp_u16b); // Read operand
		if (ret) break;
		ret = core_frame_read16(core, mem, core->cur->sp - 4, &temp_u16); // Read operand
		if (ret) break;
		if (temp_u16b == 0) {
			ret = STINT_DIV_BY_ZERO;
			break;
		}
		ret = core_frame_write16(core, mem, core->cur->sp - 4, temp_u16 % temp_u16b); // Write remainder
		if (ret) break;
		core->cur->sp -= 2;
		break;
	case op_modu32:
		core->pc += 1;
		ret = core_frame_read32(core, mem, core->cur->sp - 4, &temp_u32b); // Read operand
		if (ret) break;
		ret = core_frame_read32(core, mem, core->cur->sp - 8, &temp_u32); // Read operand
		if (ret) break;
		if (temp_u32b == 0) {
			ret = STINT_DIV_BY_ZERO;
			break;
		}
		ret = core_frame_write32(core, mem, core->cur->sp - 8, temp_u32 % temp_u32b); // Write remainder
		if (ret) break;
		core->cur->sp -= 4;
		break;
	case op_modu64:
		core->pc += 1;
		ret = core_frame_read64(core, mem, core->cur->sp - 8, &temp_u64b); // Read operand
		if (ret) break;
		ret = core_frame_read64(core, mem, core->cur->sp - 16, &temp_u64); // Read operand
		if (ret) break;
		if (temp_u64b == 0) {
			ret = STINT_DIV_BY_ZERO;
			break;
		}
		ret = core_frame_write64(core, mem, core->cur->sp - 16, temp_u64 % temp_u64b); // Write remainder
		if (ret) break;
		core->cur->sp -= 8;
		break;
	case op_modru8:
		core->pc += 1;
		ret = core_frame_read8(core, mem, core->cur->sp - 1, &temp_u8b); // Read operand
		if (ret) break;
		ret = core_frame_read8(core, mem, core->cur->sp - 2, &temp_u8); // Read operand
		if (ret) break;
		if (temp_u8 == 0) {
			ret = STINT_DIV_BY_ZERO;
			break;
		}
		ret = core_frame_write8(core, mem, core->cur->sp - 2, temp_u8b % temp_u8); // Write remainder
		if (ret) break;
		core->cur->sp -= 1;
		break;
	case op_modru16:
		core->pc += 1;
		ret = core_frame_read16(core, mem, core->cur->sp - 2, &temp_u16b); // Read operand
		if (ret) break;
		ret = core_frame_read16(core, mem, core->cur->sp - 4, &temp_u16); // Read operand
		if (ret) break;
		if (temp_u16 == 0) {
			ret = STINT_DIV_BY_ZERO;
			break;
		}
		ret = core_frame_write16(core, mem, core->cur->sp - 4, temp_u16b % temp_u16); // Write remainder
		if (ret) break;
		core->cur->sp -= 2;
		break;
	case op_modru32:
		core->pc += 1;
		ret = core_frame_read32(core, mem, core->cur->sp - 4, &temp_u32b); // Read operand
		if (ret) break;
		ret = core_frame_read32(core, mem, core->cur->sp - 8, &temp_u32); // Read operand
		if (ret) break;
		if (temp_u32 == 0) {
			ret = STINT_DIV_BY_ZERO;
			break;
		}
		ret = core_frame_write32(core, mem, core->cur->sp - 8, temp_u32b % temp_u32); // Write remainder
		if (ret) break;
		core->cur->sp -= 4;
		break;
	case op_modru64:
		core->pc += 1;
		ret = core_frame_read64(core, mem, core->cur->sp - 8, &temp_u64b); // Read operand
		if (ret) break;
		ret = core_frame_read64(core, mem, core->cur->sp - 16, &temp_u64); // Read operand
		if (ret) break;
		if (temp_u64 == 0) {
			ret = STINT_DIV_BY_ZERO;
			break;
		}
		ret = core_frame_write64(core, mem, core->cur->sp - 16, temp_u64b % temp_u64); // Write remainder
		if (ret) break;
		core->cur->sp -= 8;
		break;
	case op_modi8:
		core->pc += 1;
		ret = core_frame_read8(core, mem, core->cur->sp - 1, &temp_u8b); // Read operand
		if (ret) break;
		ret = core_frame_read8(core, mem, core->cur->sp - 2, &temp_u8); // Read operand
		if (ret) break;
		if (temp_u8b == 0) {
			ret = STINT_DIV_BY_ZERO;
			break;
		}
		ret = core_frame_write8(core, mem, core->cur->sp - 2, (int8_t)temp_u8 % (int8_t)temp_u8b); // Write remainder
		if (ret) break;
		core->cur->sp -= 1;
		break;
	case op_modi16:
		core->pc += 1;
		ret = core_frame_read16(core, mem, core->cur->sp - 2, &temp_u16b); // Read operand
		if (ret) break;
		ret = core_frame_read16(core, mem, core->cur->sp - 4, &temp_u16); // Read operand
		if (ret) break;
		if (temp_u16b == 0) {
			ret = STINT_DIV_BY_ZERO;
			break;
		}
		ret = core_frame_write16(core, mem, core->cur->sp - 4, (int16_t)temp_u16 % (int16_t)temp_u16b); // Write remainder
		if (ret) break;
		core->cur->sp -= 2;
		break;
	case op_modi32:
		core->pc += 1;
		ret = core_frame_read32(core, mem, core->cur->sp - 4, &temp_u32b); // Read operand
		if (ret) break;
		ret = core_frame_read32(core, mem, core->cur->sp - 8, &temp_u32); // Read operand
		if (ret) break;
		if (temp_u32b == 0) {
			ret = STINT_DIV_BY_ZERO;
			break;
		}
		ret = core_frame_write32(core, mem, core->cur->sp - 8, (int32_t)temp_u32 % (int32_t)temp_u32b); // Write remainder
		if (ret) break;
		core->cur->sp -= 4;
		break;
	case op_modi64:
		core->pc += 1;
		ret = core_frame_read64(core, mem, core->cur->sp - 8, &temp_u64b); // Read operand
		if (ret) break;
		ret = core_frame_read64(core, mem, core->cur->sp - 16, &temp_u64); // Read operand
		if (ret) break;
		if (temp_u64b == 0) {
			ret = STINT_DIV_BY_ZERO;
			break;
		}
		ret = core_frame_write64(core, mem, core->cur->sp - 16, (int64_t)temp_u64 % (int64_t)temp_u64b); // Write remainder
		if (ret) break;
		core->cur->sp -= 8;
		break;
	case op_modri8:
		core->pc += 1;
		ret = core_frame_read8(core, mem, core->cur->sp - 1, &temp_u8b); // Read operand
		if (ret) break;
		ret = core_frame_read8(core, mem, core->cur->sp - 2, &temp_u8); // Read operand
		if (ret) break;
		if (temp_u8 == 0) {
			ret = STINT_DIV_BY_ZERO;
			break;
		}
		ret = core_frame_write8(core, mem, core->cur->sp - 2, (int8_t)temp_u8b % (int8_t)temp_u8); // Write remainder
		if (ret) break;
		core->cur->sp -= 1;
		break;
	case op_modri16:
		core->pc += 1;
		ret = core_frame_read16(core, mem, core->cur->sp - 2, &temp_u16b); // Read operand
		if (ret) break;
		ret = core_frame_read16(core, mem, core->cur->sp - 4, &temp_u16); // Read operand
		if (ret) break;
		if (temp_u16 == 0) {
			ret = STINT_DIV_BY_ZERO;
			break;
		}
		ret = core_frame_write16(core, mem, core->cur->sp - 4, (int16_t)temp_u16b % (int16_t)temp_u16); // Write remainder
		if (ret) break;
		core->cur->sp -= 2;
		break;
	case op_modri32:
		core->pc += 1;
		ret = core_frame_read32(core, mem, core->cur->sp - 4, &temp_u32b); // Read operand
		if (ret) break;
		ret = core_frame_read32(core, mem, core->cur->sp - 8, &temp_u32); // Read operand
		if (ret) break;
		if (temp_u32 == 0) {
			ret = STINT_DIV_BY_ZERO;
			break;
		}
		ret = core_frame_write32(core, mem, core->cur->sp - 8, (int32_t)temp_u32b % (int32_t)temp_u32); // Write remainder
		if (ret) break;
		core->cur->sp -= 4;
		break;
	case op_modri64:
		core->pc += 1;
		ret = core_frame_read64(core, mem, core->cur->sp - 8, &temp_u64b); // Read operand
		if (ret) break;
		ret = core_frame_read64(core, mem, core->cur->sp - 16, &temp_u64); // Read operand
		if (ret) break;
		if (temp_u64 == 0) {
			ret = STINT_DIV_BY_ZERO;
			break;
		}
		ret = core_frame_write64(core, mem, core->cur->sp - 16, (int64_t)temp_u64b % (int64_t)temp_u64); // Write remainder
		if (ret) break;
		core->cur->sp -= 8;
		break;

	//
	// Bitwise shift operations
	//
	case op_lshift8:
		core->pc += 1;
		ret = core_frame_read8(core, mem, core->cur->sp - 1, &temp_u8b); // Read operand
		if (ret) break;
		ret = core_frame_read8(core, mem, core->cur->sp - 2, &temp_u8); // Read operand
		if (ret) break;
		ret = core_frame_write8(core, mem, core->cur->sp - 2, temp_u8 << temp_u8b); // Write result
		if (ret) break;
		core->cur->sp -= 1;
		break;
	case op_lshift16:
		core->pc += 1;
		ret = core_frame_read8(core, mem, core->cur->sp - 1, &temp_u8b); // Read operand
		if (ret) break;
		ret = core_frame_read16(core, mem, core->cur->sp - 3, &temp_u16); // Read operand
		if (ret) break;
		ret = core_frame_write16(core, mem, core->cur->sp - 3, temp_u16 << temp_u8b); // Write result
		if (ret) break;
		core->cur->sp -= 1;
		break;
	case op_lshift32:
		core->pc += 1;
		ret = core_frame_read8(core, mem, core->cur->sp - 1, &temp_u8b); // Read operand
		if (ret) break;
		ret = core_frame_read32(core, mem, core->cur->sp - 5, &temp_u32); // Read operand
		if (ret) break;
		ret = core_frame_write32(core, mem, core->cur->sp - 5, temp_u32 << temp_u8b); // Write result
		if (ret) break;
		core->cur->sp -= 1;
		break;
	case op_lshift64:
		core->pc += 1;
		ret = core_frame_read8(core, mem, core->cur->sp - 1, &temp_u8b); // Read operand
		if (ret) break;
		ret = core_frame_read64(core, mem, core->cur->sp - 9, &temp_u64); // Read operand
		if (ret) break;
		ret = core_frame_write64(core, mem, core->cur->sp - 9, temp_u64 << temp_u8b); // Write result
		if (ret) break;
		core->cur->sp -= 1;
		break;
	case op_rshiftu8:
		core->pc += 1;
		ret = core_frame_read8(core, mem, core->cur->sp - 1, &temp_u8b); // Read operand
		if (ret) break;
		ret = core_frame_read8(core, mem, core->cur->sp - 2, &temp_u8); // Read operand
		if (ret) break;
		ret = core_frame_write8(core, mem, core->cur->sp - 2, temp_u8 >> temp_u8b); // Write result
		if (ret) break;
		core->cur->sp -= 1;
		break;
	case op_rshiftu16:
		core->pc += 1;
		ret = core_frame_read8(core, mem, core->cur->sp - 1, &temp_u8b); // Read operand
		if (ret) break;
		ret = core_frame_read16(core, mem, core->cur->sp - 3, &temp_u16); // Read operand
		if (ret) break;
		ret = core_frame_write16(core, mem, core->cur->sp - 3, temp_u16 >> temp_u8b); // Write result
		if (ret) break;
		core->cur->sp -= 1;
		break;
	case op_rshiftu32:
		core->pc += 1;
		ret = core_frame_read8(core, mem, core->cur->sp - 1, &temp_u8b); // Read operand
		if (ret) break;
		ret = core_frame_read32(core, mem, core->cur->sp - 5, &temp_u32); // Read operand
		if (ret) break;
		ret = core_frame_write32(core, mem, core->cur->sp - 5, temp_u32 >> temp_u8b); // Write result
		if (ret) break;
		core->cur->sp -= 1;
		break;
	case op_rshiftu64:
		core->pc += 1;
		ret = core_frame_read8(core, mem, core->cur->sp - 1, &temp_u8b); // Read operand
		if (ret) break;
		ret = core_frame_read64(core, mem, core->cur->sp - 9, &temp_u64); // Read operand
		if (ret) break;
		ret = core_frame_write64(core, mem, core->cur->sp - 9, temp_u64 >> temp_u8b); // Write result
		if (ret) break;
		core->cur->sp -= 1;
		break;
	case op_rshifti8:
		core->pc += 1;
		ret = core_frame_read8(core, mem, core->cur->sp - 1, &temp_u8b); // Read operand
		if (ret) break;
		ret = core_frame_read8(core, mem, core->cur->sp - 2, &temp_u8); // Read operand
		if (ret) break;
		ret = core_frame_write8(core, mem, core->cur->sp - 2, (int8_t)temp_u8 >> temp_u8b); // Write result
		if (ret) break;
		core->cur->sp -= 1;
		break;
	case op_rshifti16:
		core->pc += 1;
		ret = core_frame_read8(core, mem, core->cur->sp - 1, &temp_u8b); // Read operand
		if (ret) break;
		ret = core_frame_read16(core, mem, core->cur->sp - 3, &temp_u16); // Read operand
		if (ret) break;
		ret = core_frame_write16(core, mem, core->cur->sp - 3, (int16_t)temp_u16 >> temp_u8b); // Write result
		if (ret) break;
		core->cur->sp -= 1;
		break;
	case op_rshifti32:
		core->pc += 1;
		ret = core_frame_read8(core, mem, core->cur->sp - 1, &temp_u8b); // Read operand
		if (ret) break;
		ret = core_frame_read32(core, mem, core->cur->sp - 5, &temp_u32); // Read operand
		if (ret) break;
		ret = core_frame_write32(core, mem, core->cur->sp - 5, (int32_t)temp_u32 >> temp_u8b); // Write result
		if (ret) break;
		core->cur->sp -= 1;
		break;
	case op_rshifti64:
		core->pc += 1;
		ret = core_frame_read8(core, mem, core->cur->sp - 1, &temp_u8b); // Read operand
		if (ret) break;
		ret = core_frame_read64(core, mem, core->cur->sp - 9, &temp_u64); // Read operand
		if (ret) break;
		ret = core_frame_write64(core, mem, core->cur->sp - 9, (int64_t)temp_u64 >> temp_u8b); // Write result
		if (ret) break;
		core->cur->sp -= 1;
		break;

	//
	// Bitwise logical operations
	//
	case op_band8:
		core->pc += 1;
		ret = core_frame_read8(core, mem, core->cur->sp - 1, &temp_u8b); // Read operand
		if (ret) break;
		ret = core_frame_read8(core, mem, core->cur->sp - 2, &temp_u8); // Read operand
		if (ret) break;
		ret = core_frame_write8(core, mem, core->cur->sp - 2, temp_u8 & temp_u8b); // Write result
		if (ret) break;
		core->cur->sp -= 1;
		break;
	case op_band16:
		core->pc += 1;
		ret = core_frame_read16(core, mem, core->cur->sp - 2, &temp_u16b); // Read operand
		if (ret) break;
		ret = core_frame_read16(core, mem, core->cur->sp - 4, &temp_u16); // Read operand
		if (ret) break;
		ret = core_frame_write16(core, mem, core->cur->sp - 4, temp_u16 & temp_u16b); // Write result
		if (ret) break;
		core->cur->sp -= 2;
		break;
	case op_band32:
		core->pc += 1;
		ret = core_frame_read32(core, mem, core->cur->sp - 4, &temp_u32b); // Read operand
		if (ret) break;
		ret = core_frame_read32(core, mem, core->cur->sp - 8, &temp_u32); // Read operand
		if (ret) break;
		ret = core_frame_write32(core, mem, core->cur->sp - 8, temp_u32 & temp_u32b); // Write result
		if (ret) break;
		core->cur->sp -= 4;
		break;
	case op_band64:
		core->pc += 1;
		ret = core_frame_read64(core, mem, core->cur->sp - 8, &temp_u64b); // Read operand
		if (ret) break;
		ret = core_frame_read64(core, mem, core->cur->sp - 16, &temp_u64); // Read operand
		if (ret) break;
		ret = core_frame_write64(core, mem, core->cur->sp - 16, temp_u64 & temp_u64b); // Write result
		if (ret) break;
		core->cur->sp -= 8;
		break;
	case op_bor8:
		core->pc += 1;
		ret = core_frame_read8(core, mem, core->cur->sp - 1, &temp_u8b); // Read operand
		if (ret) break;
		ret = core_frame_read8(core, mem, core->cur->sp - 2, &temp_u8); // Read operand
		if (ret) break;
		ret = core_frame_write8(core, mem, core->cur->sp - 2, temp_u8 | temp_u8b); // Write result
		if (ret) break;
		core->cur->sp -= 1;
		break;
	case op_bor16:
		core->pc += 1;
		ret = core_frame_read16(core, mem, core->cur->sp - 2, &temp_u16b); // Read operand
		if (ret) break;
		ret = core_frame_read16(core, mem, core->cur->sp - 4, &temp_u16); // Read operand
		if (ret) break;
		ret = core_frame_write16(core, mem, core->cur->sp - 4, temp_u16 | temp_u16b); // Write result
		if (ret) break;
		core->cur->sp -= 2;
		break;
	case op_bor32:
		core->pc += 1;
		ret = core_frame_read32(core, mem, core->cur->sp - 4, &temp_u32b); // Read operand
		if (ret) break;
		ret = core_frame_read32(core, mem, core->cur->sp - 8, &temp_u32); // Read operand
		if (ret) break;
		ret = core_frame_write32(core, mem, core->cur->sp - 8, temp_u32 | temp_u32b); // Write result
		if (ret) break;
		core->cur->sp -= 4;
		break;
	case op_bor64:
		core->pc += 1;
		ret = core_frame_read64(core, mem, core->cur->sp - 8, &temp_u64b); // Read operand
		if (ret) break;
		ret = core_frame_read64(core, mem, core->cur->sp - 16, &temp_u64); // Read operand
		if (ret) break;
		ret = core_frame_write64(core, mem, core->cur->sp - 16, temp_u64 | temp_u64b); // Write result
		if (ret) break;
		core->cur->sp -= 8;
		break;
	case op_bxor8:
		core->pc += 1;
		ret = core_frame_read8(core, mem, core->cur->sp - 1, &temp_u8b); // Read operand
		if (ret) break;
		ret = core_frame_read8(core, mem, core->cur->sp - 2, &temp_u8); // Read operand
		if (ret) break;
		ret = core_frame_write8(core, mem, core->cur->sp - 2, temp_u8 ^ temp_u8b); // Write result
		if (ret) break;
		core->cur->sp -= 1;
		break;
	case op_bxor16:
		core->pc += 1;
		ret = core_frame_read16(core, mem, core->cur->sp - 2, &temp_u16b); // Read operand
		if (ret) break;
		ret = core_frame_read16(core, mem, core->cur->sp - 4, &temp_u16); // Read operand
		if (ret) break;
		ret = core_frame_write16(core, mem, core->cur->sp - 4, temp_u16 ^ temp_u16b); // Write result
		if (ret) break;
		core->cur->sp -= 2;
		break;
	case op_bxor32:
		core->pc += 1;
		ret = core_frame_read32(core, mem, core->cur->sp - 4, &temp_u32b); // Read operand
		if (ret) break;
		ret = core_frame_read32(core, mem, core->cur->sp - 8, &temp_u32); // Read operand
		if (ret) break;
		ret = core_frame_write32(core, mem, core->cur->sp - 8, temp_u32 ^ temp_u32b); // Write result
		if (ret) break;
		core->cur->sp -= 4;
		break;
	case op_bxor64:
		core->pc += 1;
		ret = core_frame_read64(core, mem, core->cur->sp - 8, &temp_u64b); // Read operand
		if (ret) break;
		ret = core_frame_read64(core, mem, core->cur->sp - 16, &temp_u64); // Read operand
		if (ret) break;
		ret = core_frame_write64(core, mem, core->cur->sp - 16, temp_u64 ^ temp_u64b); // Write result
		if (ret) break;
		core->cur->sp -= 8;
		break;
	case op_binv8:
		core->pc += 1;
		ret = core_frame_read8(core, mem, core->cur->sp - 1, &temp_u8); // Read operand
		if (ret) break;
		ret = core_frame_write8(core, mem, core->cur->sp - 1, ~temp_u8); // Write result
		if (ret) break;
		break;
	case op_binv16:
		core->pc += 1;
		ret = core_frame_read16(core, mem, core->cur->sp - 2, &temp_u16); // Read operand
		if (ret) break;
		ret = core_frame_write16(core, mem, core->cur->sp - 2, ~temp_u16); // Write result
		if (ret) break;
		break;
	case op_binv32:
		core->pc += 1;
		ret = core_frame_read32(core, mem, core->cur->sp - 4, &temp_u32); // Read operand
		if (ret) break;
		ret = core_frame_write32(core, mem, core->cur->sp - 4, ~temp_u32); // Write result
		if (ret) break;
		break;
	case op_binv64:
		core->pc += 1;
		ret = core_frame_read64(core, mem, core->cur->sp - 8, &temp_u64); // Read operand
		if (ret) break;
		ret = core_frame_write64(core, mem, core->cur->sp - 8, ~temp_u64); // Write result
		if (ret) break;
		break;

	//
	// Boolean logical operations
	//
	case op_land8:
		core->pc += 1;
		ret = core_frame_read8(core, mem, core->cur->sp - 1, &temp_u8b); // Read operand
		if (ret) break;
		ret = core_frame_read8(core, mem, core->cur->sp - 2, &temp_u8); // Read operand
		if (ret) break;
		ret = core_frame_write8(core, mem, core->cur->sp - 2, temp_u8 && temp_u8b); // Write result
		if (ret) break;
		core->cur->sp -= 1;
		break;
	case op_land16:
		core->pc += 1;
		ret = core_frame_read16(core, mem, core->cur->sp - 2, &temp_u16b); // Read operand
		if (ret) break;
		ret = core_frame_read16(core, mem, core->cur->sp - 4, &temp_u16); // Read operand
		if (ret) break;
		ret = core_frame_write16(core, mem, core->cur->sp - 4, temp_u16 && temp_u16b); // Write result
		if (ret) break;
		core->cur->sp -= 2;
		break;
	case op_land32:
		core->pc += 1;
		ret = core_frame_read32(core, mem, core->cur->sp - 4, &temp_u32b); // Read operand
		if (ret) break;
		ret = core_frame_read32(core, mem, core->cur->sp - 8, &temp_u32); // Read operand
		if (ret) break;
		ret = core_frame_write32(core, mem, core->cur->sp - 8, temp_u32 && temp_u32b); // Write result
		if (ret) break;
		core->cur->sp -= 4;
		break;
	case op_land64:
		core->pc += 1;
		ret = core_frame_read64(core, mem, core->cur->sp - 8, &temp_u64b); // Read operand
		if (ret) break;
		ret = core_frame_read64(core, mem, core->cur->sp - 16, &temp_u64); // Read operand
		if (ret) break;
		ret = core_frame_write64(core, mem, core->cur->sp - 16, temp_u64 && temp_u64b); // Write result
		if (ret) break;
		core->cur->sp -= 8;
		break;
	case op_lor8:
		core->pc += 1;
		ret = core_frame_read8(core, mem, core->cur->sp - 1, &temp_u8b); // Read operand
		if (ret) break;
		ret = core_frame_read8(core, mem, core->cur->sp - 2, &temp_u8); // Read operand
		if (ret) break;
		ret = core_frame_write8(core, mem, core->cur->sp - 2, temp_u8 || temp_u8b); // Write result
		if (ret) break;
		core->cur->sp -= 1;
		break;
	case op_lor16:
		core->pc += 1;
		ret = core_frame_read16(core, mem, core->cur->sp - 2, &temp_u16b); // Read operand
		if (ret) break;
		ret = core_frame_read16(core, mem, core->cur->sp - 4, &temp_u16); // Read operand
		if (ret) break;
		ret = core_frame_write16(core, mem, core->cur->sp - 4, temp_u16 || temp_u16b); // Write result
		if (ret) break;
		core->cur->sp -= 2;
		break;
	case op_lor32:
		core->pc += 1;
		ret = core_frame_read32(core, mem, core->cur->sp - 4, &temp_u32b); // Read operand
		if (ret) break;
		ret = core_frame_read32(core, mem, core->cur->sp - 8, &temp_u32); // Read operand
		if (ret) break;
		ret = core_frame_write32(core, mem, core->cur->sp - 8, temp_u32 || temp_u32b); // Write result
		if (ret) break;
		core->cur->sp -= 4;
		break;
	case op_lor64:
		core->pc += 1;
		ret = core_frame_read64(core, mem, core->cur->sp - 8, &temp_u64b); // Read operand
		if (ret) break;
		ret = core_frame_read64(core, mem, core->cur->sp - 16, &temp_u64); // Read operand
		if (ret) break;
		ret = core_frame_write64(core, mem, core->cur->sp - 16, temp_u64 || temp_u64b); // Write result
		if (ret) break;
		core->cur->sp -= 8;
		break;
	case op_linv8:
		core->pc += 1;
		ret = core_frame_read8(core, mem, core->cur->sp - 1, &temp_u8); // Read operand
		if (ret) break;
		ret = core_frame_write8(core, mem, core->cur->sp - 1, !temp_u8); // Write result
		if (ret) break;
		break;
	case op_linv16:
		core->pc += 1;
		ret = core_frame_read16(core, mem, core->cur->sp - 2, &temp_u16); // Read operand
		if (ret) break;
		ret = core_frame_write16(core, mem, core->cur->sp - 2, !temp_u16); // Write result
		if (ret) break;
		break;
	case op_linv32:
		core->pc += 1;
		ret = core_frame_read32(core, mem, core->cur->sp - 4, &temp_u32); // Read operand
		if (ret) break;
		ret = core_frame_write32(core, mem, core->cur->sp - 4, !temp_u32); // Write result
		if (ret) break;
		break;
	case op_linv64:
		core->pc += 1;
		ret = core_frame_read64(core, mem, core->cur->sp - 8, &temp_u64); // Read operand
		if (ret) break;
		ret = core_frame_write64(core, mem, core->cur->sp - 8, !temp_u64); // Write result
		if (ret) break;
		break;

	//
	// Comparison operations
	//
	case op_ceq8:
		core->pc += 1;
		ret = core_frame_read8(core, mem, core->cur->sp - 1, &temp_u8b); // Read operand
		if (ret) break;
		ret = core_frame_read8(core, mem, core->cur->sp - 2, &temp_u8); // Read operand
		if (ret) break;
		ret = core_frame_write8(core, mem, core->cur->sp - 2, temp_u8 == temp_u8b); // Write result
		if (ret) break;
		core->cur->sp -= 1;
		break;
	case op_ceq16:
		core->pc += 1;
		ret = core_frame_read16(core, mem, core->cur->sp - 2, &temp_u16b); // Read operand
		if (ret) break;
		ret = core_frame_read16(core, mem, core->cur->sp - 4, &temp_u16); // Read operand
		if (ret) break;
		ret = core_frame_write16(core, mem, core->cur->sp - 4, temp_u16 == temp_u16b); // Write result
		if (ret) break;
		core->cur->sp -= 2;
		break;
	case op_ceq32:
		core->pc += 1;
		ret = core_frame_read32(core, mem, core->cur->sp - 4, &temp_u32b); // Read operand
		if (ret) break;
		ret = core_frame_read32(core, mem, core->cur->sp - 8, &temp_u32); // Read operand
		if (ret) break;
		ret = core_frame_write32(core, mem, core->cur->sp - 8, temp_u32 == temp_u32b); // Write result
		if (ret) break;
		core->cur->sp -= 4;
		break;
	case op_ceq64:
		core->pc += 1;
		ret = core_frame_read64(core, mem, core->cur->sp - 8, &temp_u64b); // Read operand
		if (ret) break;
		ret = core_frame_read64(core, mem, core->cur->sp - 16, &temp_u64); // Read operand
		if (ret) break;
		ret = core_frame_write64(core, mem, core->cur->sp - 16, temp_u64 == temp_u64b); // Write result
		if (ret) break;
		core->cur->sp -= 8;
		break;
	case op_cne8:
		core->pc += 1;
		ret = core_frame_read8(core, mem, core->cur->sp - 1, &temp_u8b); // Read operand
		if (ret) break;
		ret = core_frame_read8(core, mem, core->cur->sp - 2, &temp_u8); // Read operand
		if (ret) break;
		ret = core_frame_write8(core, mem, core->cur->sp - 2, temp_u8 != temp_u8b); // Write result
		if (ret) break;
		core->cur->sp -= 1;
		break;
	case op_cne16:
		core->pc += 1;
		ret = core_frame_read16(core, mem, core->cur->sp - 2, &temp_u16b); // Read operand
		if (ret) break;
		ret = core_frame_read16(core, mem, core->cur->sp - 4, &temp_u16); // Read operand
		if (ret) break;
		ret = core_frame_write16(core, mem, core->cur->sp - 4, temp_u16 != temp_u16b); // Write result
		if (ret) break;
		core->cur->sp -= 2;
		break;
	case op_cne32:
		core->pc += 1;
		ret = core_frame_read32(core, mem, core->cur->sp - 4, &temp_u32b); // Read operand
		if (ret) break;
		ret = core_frame_read32(core, mem, core->cur->sp - 8, &temp_u32); // Read operand
		if (ret) break;
		ret = core_frame_write32(core, mem, core->cur->sp - 8, temp_u32 != temp_u32b); // Write result
		if (ret) break;
		core->cur->sp -= 4;
		break;
	case op_cne64:
		core->pc += 1;
		ret = core_frame_read64(core, mem, core->cur->sp - 8, &temp_u64b); // Read operand
		if (ret) break;
		ret = core_frame_read64(core, mem, core->cur->sp - 16, &temp_u64); // Read operand
		if (ret) break;
		ret = core_frame_write64(core, mem, core->cur->sp - 16, temp_u64 != temp_u64b); // Write result
		if (ret) break;
		core->cur->sp -= 8;
		break;
	case op_cgtu8:
		core->pc += 1;
		ret = core_frame_read8(core, mem, core->cur->sp - 1, &temp_u8b); // Read operand
		if (ret) break;
		ret = core_frame_read8(core, mem, core->cur->sp - 2, &temp_u8); // Read operand
		if (ret) break;
		ret = core_frame_write8(core, mem, core->cur->sp - 2, temp_u8 > temp_u8b); // Write result
		if (ret) break;
		core->cur->sp -= 1;
		break;
	case op_cgtu16:
		core->pc += 1;
		ret = core_frame_read16(core, mem, core->cur->sp - 2, &temp_u16b); // Read operand
		if (ret) break;
		ret = core_frame_read16(core, mem, core->cur->sp - 4, &temp_u16); // Read operand
		if (ret) break;
		ret = core_frame_write16(core, mem, core->cur->sp - 4, temp_u16 > temp_u16b); // Write result
		if (ret) break;
		core->cur->sp -= 2;
		break;
	case op_cgtu32:
		core->pc += 1;
		ret = core_frame_read32(core, mem, core->cur->sp - 4, &temp_u32b); // Read operand
		if (ret) break;
		ret = core_frame_read32(core, mem, core->cur->sp - 8, &temp_u32); // Read operand
		if (ret) break;
		ret = core_frame_write32(core, mem, core->cur->sp - 8, temp_u32 > temp_u32b); // Write result
		if (ret) break;
		core->cur->sp -= 4;
		break;
	case op_cgtu64:
		core->pc += 1;
		ret = core_frame_read64(core, mem, core->cur->sp - 8, &temp_u64b); // Read operand
		if (ret) break;
		ret = core_frame_read64(core, mem, core->cur->sp - 16, &temp_u64); // Read operand
		if (ret) break;
		ret = core_frame_write64(core, mem, core->cur->sp - 16, temp_u64 > temp_u64b); // Write result
		if (ret) break;
		core->cur->sp -= 8;
		break;
	case op_cgti8:
		core->pc += 1;
		ret = core_frame_read8(core, mem, core->cur->sp - 1, &temp_u8b); // Read operand
		if (ret) break;
		ret = core_frame_read8(core, mem, core->cur->sp - 2, &temp_u8); // Read operand
		if (ret) break;
		ret = core_frame_write8(core, mem, core->cur->sp - 2, (int8_t)temp_u8 > (int8_t)temp_u8b); // Write result
		if (ret) break;
		core->cur->sp -= 1;
		break;
	case op_cgti16:
		core->pc += 1;
		ret = core_frame_read16(core, mem, core->cur->sp - 2, &temp_u16b); // Read operand
		if (ret) break;
		ret = core_frame_read16(core, mem, core->cur->sp - 4, &temp_u16); // Read operand
		if (ret) break;
		ret = core_frame_write16(core, mem, core->cur->sp - 4, (int16_t)temp_u16 > (int16_t)temp_u16b); // Write result
		if (ret) break;
		core->cur->sp -= 2;
		break;
	case op_cgti32:
		core->pc += 1;
		ret = core_frame_read32(core, mem, core->cur->sp - 4, &temp_u32b); // Read operand
		if (ret) break;
		ret = core_frame_read32(core, mem, core->cur->sp - 8, &temp_u32); // Read operand
		if (ret) break;
		ret = core_frame_write32(core, mem, core->cur->sp - 8, (int32_t)temp_u32 > (int32_t)temp_u32b); // Write result
		if (ret) break;
		core->cur->sp -= 4;
		break;
	case op_cgti64:
		core->pc += 1;
		ret = core_frame_read64(core, mem, core->cur->sp - 8, &temp_u64b); // Read operand
		if (ret) break;
		ret = core_frame_read64(core, mem, core->cur->sp - 16, &temp_u64); // Read operand
		if (ret) break;
		ret = core_frame_write64(core, mem, core->cur->sp - 16, (int64_t)temp_u64 > (int64_t)temp_u64b); // Write result
		if (ret) break;
		core->cur->sp -= 8;
		break;
	case op_cltu8:
		core->pc += 1;
		ret = core_frame_read8(core, mem, core->cur->sp - 1, &temp_u8b); // Read operand
		if (ret) break;
		ret = core_frame_read8(core, mem, core->cur->sp - 2, &temp_u8); // Read operand
		if (ret) break;
		ret = core_frame_write8(core, mem, core->cur->sp - 2, temp_u8 < temp_u8b); // Write result
		if (ret) break;
		core->cur->sp -= 1;
		break;
	case op_cltu16:
		core->pc += 1;
		ret = core_frame_read16(core, mem, core->cur->sp - 2, &temp_u16b); // Read operand
		if (ret) break;
		ret = core_frame_read16(core, mem, core->cur->sp - 4, &temp_u16); // Read operand
		if (ret) break;
		ret = core_frame_write16(core, mem, core->cur->sp - 4, temp_u16 < temp_u16b); // Write result
		if (ret) break;
		core->cur->sp -= 2;
		break;
	case op_cltu32:
		core->pc += 1;
		ret = core_frame_read32(core, mem, core->cur->sp - 4, &temp_u32b); // Read operand
		if (ret) break;
		ret = core_frame_read32(core, mem, core->cur->sp - 8, &temp_u32); // Read operand
		if (ret) break;
		ret = core_frame_write32(core, mem, core->cur->sp - 8, temp_u32 < temp_u32b); // Write result
		if (ret) break;
		core->cur->sp -= 4;
		break;
	case op_cltu64:
		core->pc += 1;
		ret = core_frame_read64(core, mem, core->cur->sp - 8, &temp_u64b); // Read operand
		if (ret) break;
		ret = core_frame_read64(core, mem, core->cur->sp - 16, &temp_u64); // Read operand
		if (ret) break;
		ret = core_frame_write64(core, mem, core->cur->sp - 16, temp_u64 < temp_u64b); // Write result
		if (ret) break;
		core->cur->sp -= 8;
		break;
	case op_clti8:
		core->pc += 1;
		ret = core_frame_read8(core, mem, core->cur->sp - 1, &temp_u8b); // Read operand
		if (ret) break;
		ret = core_frame_read8(core, mem, core->cur->sp - 2, &temp_u8); // Read operand
		if (ret) break;
		ret = core_frame_write8(core, mem, core->cur->sp - 2, (int8_t)temp_u8 < (int8_t)temp_u8b); // Write result
		if (ret) break;
		core->cur->sp -= 1;
		break;
	case op_clti16:
		core->pc += 1;
		ret = core_frame_read16(core, mem, core->cur->sp - 2, &temp_u16b); // Read operand
		if (ret) break;
		ret = core_frame_read16(core, mem, core->cur->sp - 4, &temp_u16); // Read operand
		if (ret) break;
		ret = core_frame_write16(core, mem, core->cur->sp - 4, (int16_t)temp_u16 < (int16_t)temp_u16b); // Write result
		if (ret) break;
		core->cur->sp -= 2;
		break;
	case op_clti32:
		core->pc += 1;
		ret = core_frame_read32(core, mem, core->cur->sp - 4, &temp_u32b); // Read operand
		if (ret) break;
		ret = core_frame_read32(core, mem, core->cur->sp - 8, &temp_u32); // Read operand
		if (ret) break;
		ret = core_frame_write32(core, mem, core->cur->sp - 8, (int32_t)temp_u32 < (int32_t)temp_u32b); // Write result
		if (ret) break;
		core->cur->sp -= 4;
		break;
	case op_clti64:
		core->pc += 1;
		ret = core_frame_read64(core, mem, core->cur->sp - 8, &temp_u64b); // Read operand
		if (ret) break;
		ret = core_frame_read64(core, mem, core->cur->sp - 16, &temp_u64); // Read operand
		if (ret) break;
		ret = core_frame_write64(core, mem, core->cur->sp - 16, (int64_t)temp_u64 < (int64_t)temp_u64b); // Write result
		if (ret) break;
		core->cur->sp -= 8;
		break;
	case op_cgeu8:
		core->pc += 1;
		ret = core_frame_read8(core, mem, core->cur->sp - 1, &temp_u8b); // Read operand
		if (ret) break;
		ret = core_frame_read8(core, mem, core->cur->sp - 2, &temp_u8); // Read operand
		if (ret) break;
		ret = core_frame_write8(core, mem, core->cur->sp - 2, temp_u8 >= temp_u8b); // Write result
		if (ret) break;
		core->cur->sp -= 1;
		break;
	case op_cgeu16:
		core->pc += 1;
		ret = core_frame_read16(core, mem, core->cur->sp - 2, &temp_u16b); // Read operand
		if (ret) break;
		ret = core_frame_read16(core, mem, core->cur->sp - 4, &temp_u16); // Read operand
		if (ret) break;
		ret = core_frame_write16(core, mem, core->cur->sp - 4, temp_u16 >= temp_u16b); // Write result
		if (ret) break;
		core->cur->sp -= 2;
		break;
	case op_cgeu32:
		core->pc += 1;
		ret = core_frame_read32(core, mem, core->cur->sp - 4, &temp_u32b); // Read operand
		if (ret) break;
		ret = core_frame_read32(core, mem, core->cur->sp - 8, &temp_u32); // Read operand
		if (ret) break;
		ret = core_frame_write32(core, mem, core->cur->sp - 8, temp_u32 >= temp_u32b); // Write result
		if (ret) break;
		core->cur->sp -= 4;
		break;
	case op_cgeu64:
		core->pc += 1;
		ret = core_frame_read64(core, mem, core->cur->sp - 8, &temp_u64b); // Read operand
		if (ret) break;
		ret = core_frame_read64(core, mem, core->cur->sp - 16, &temp_u64); // Read operand
		if (ret) break;
		ret = core_frame_write64(core, mem, core->cur->sp - 16, temp_u64 >= temp_u64b); // Write result
		if (ret) break;
		core->cur->sp -= 8;
		break;
	case op_cgei8:
		core->pc += 1;
		ret = core_frame_read8(core, mem, core->cur->sp - 1, &temp_u8b); // Read operand
		if (ret) break;
		ret = core_frame_read8(core, mem, core->cur->sp - 2, &temp_u8); // Read operand
		if (ret) break;
		ret = core_frame_write8(core, mem, core->cur->sp - 2, (int8_t)temp_u8 >= (int8_t)temp_u8b); // Write result
		if (ret) break;
		core->cur->sp -= 1;
		break;
	case op_cgei16:
		core->pc += 1;
		ret = core_frame_read16(core, mem, core->cur->sp - 2, &temp_u16b); // Read operand
		if (ret) break;
		ret = core_frame_read16(core, mem, core->cur->sp - 4, &temp_u16); // Read operand
		if (ret) break;
		ret = core_frame_write16(core, mem, core->cur->sp - 4, (int16_t)temp_u16 >= (int16_t)temp_u16b); // Write result
		if (ret) break;
		core->cur->sp -= 2;
		break;
	case op_cgei32:
		core->pc += 1;
		ret = core_frame_read32(core, mem, core->cur->sp - 4, &temp_u32b); // Read operand
		if (ret) break;
		ret = core_frame_read32(core, mem, core->cur->sp - 8, &temp_u32); // Read operand
		if (ret) break;
		ret = core_frame_write32(core, mem, core->cur->sp - 8, (int32_t)temp_u32 >= (int32_t)temp_u32b); // Write result
		if (ret) break;
		core->cur->sp -= 4;
		break;
	case op_cgei64:
		core->pc += 1;
		ret = core_frame_read64(core, mem, core->cur->sp - 8, &temp_u64b); // Read operand
		if (ret) break;
		ret = core_frame_read64(core, mem, core->cur->sp - 16, &temp_u64); // Read operand
		if (ret) break;
		ret = core_frame_write64(core, mem, core->cur->sp - 16, (int64_t)temp_u64 >= (int64_t)temp_u64b); // Write result
		if (ret) break;
		core->cur->sp -= 8;
		break;
	case op_cleu8:
		core->pc += 1;
		ret = core_frame_read8(core, mem, core->cur->sp - 1, &temp_u8b); // Read operand
		if (ret) break;
		ret = core_frame_read8(core, mem, core->cur->sp - 2, &temp_u8); // Read operand
		if (ret) break;
		ret = core_frame_write8(core, mem, core->cur->sp - 2, temp_u8 <= temp_u8b); // Write result
		if (ret) break;
		core->cur->sp -= 1;
		break;
	case op_cleu16:
		core->pc += 1;
		ret = core_frame_read16(core, mem, core->cur->sp - 2, &temp_u16b); // Read operand
		if (ret) break;
		ret = core_frame_read16(core, mem, core->cur->sp - 4, &temp_u16); // Read operand
		if (ret) break;
		ret = core_frame_write16(core, mem, core->cur->sp - 4, temp_u16 <= temp_u16b); // Write result
		if (ret) break;
		core->cur->sp -= 2;
		break;
	case op_cleu32:
		core->pc += 1;
		ret = core_frame_read32(core, mem, core->cur->sp - 4, &temp_u32b); // Read operand
		if (ret) break;
		ret = core_frame_read32(core, mem, core->cur->sp - 8, &temp_u32); // Read operand
		if (ret) break;
		ret = core_frame_write32(core, mem, core->cur->sp - 8, temp_u32 <= temp_u32b); // Write result
		if (ret) break;
		core->cur->sp -= 4;
		break;
	case op_cleu64:
		core->pc += 1;
		ret = core_frame_read64(core, mem, core->cur->sp - 8, &temp_u64b); // Read operand
		if (ret) break;
		ret = core_frame_read64(core, mem, core->cur->sp - 16, &temp_u64); // Read operand
		if (ret) break;
		ret = core_frame_write64(core, mem, core->cur->sp - 16, temp_u64 <= temp_u64b); // Write result
		if (ret) break;
		core->cur->sp -= 8;
		break;
	case op_clei8:
		core->pc += 1;
		ret = core_frame_read8(core, mem, core->cur->sp - 1, &temp_u8b); // Read operand
		if (ret) break;
		ret = core_frame_read8(core, mem, core->cur->sp - 2, &temp_u8); // Read operand
		if (ret) break;
		ret = core_frame_write8(core, mem, core->cur->sp - 2, (int8_t)temp_u8 <= (int8_t)temp_u8b); // Write result
		if (ret) break;
		core->cur->sp -= 1;
		break;
	case op_clei16:
		core->pc += 1;
		ret = core_frame_read16(core, mem, core->cur->sp - 2, &temp_u16b); // Read operand
		if (ret) break;
		ret = core_frame_read16(core, mem, core->cur->sp - 4, &temp_u16); // Read operand
		if (ret) break;
		ret = core_frame_write16(core, mem, core->cur->sp - 4, (int16_t)temp_u16 <= (int16_t)temp_u16b); // Write result
		if (ret) break;
		core->cur->sp -= 2;
		break;
	case op_clei32:
		core->pc += 1;
		ret = core_frame_read32(core, mem, core->cur->sp - 4, &temp_u32b); // Read operand
		if (ret) break;
		ret = core_frame_read32(core, mem, core->cur->sp - 8, &temp_u32); // Read operand
		if (ret) break;
		ret = core_frame_write32(core, mem, core->cur->sp - 8, (int32_t)temp_u32 <= (int32_t)temp_u32b); // Write result
		if (ret) break;
		core->cur->sp -= 4;
		break;
	case op_clei64:
		core->pc += 1;
		ret = core_frame_read64(core, mem, core->cur->sp - 8, &temp_u64b); // Read operand
		if (ret) break;
		ret = core_frame_read64(core, mem, core->cur->sp - 16, &temp_u64); // Read operand
		if (ret) break;
		ret = core_frame_write64(core, mem, core->cur->sp - 16, (int64_t)temp_u64 <= (int64_t)temp_u64b); // Write result
		if (ret) break;
		core->cur->sp -= 8;
		break;

	//
	// Function operations
	//
	case op_call:
		ret = core_mem_read64(core, mem, core->pc + 1, &temp_u64); // Read imm address
		if (ret) break;
		ret = core_frame_write64(core, mem, core->cur->sp, core->cur->sfp); // Push SFP
		if (ret) break;
		ret = core_frame_write64(core, mem, core->cur->sp + 8, core->pc + 9); // Push RETA
		if (ret) break;
		core->cur->sp += STACK_FRAME_METADATA_SIZE;
		core->cur->sfp = core->cur->sp;
		core->pc = temp_u64;
		break;
	case op_calls:
		ret = core_frame_read64(core, mem, core->cur->sp - 8, &temp_u64); // Read and pop address
		if (ret) break;
		ret = core_frame_write64(core, mem, core->cur->sp - 8, core->cur->sfp); // Push SFP
		if (ret) break;
		ret = core_frame_write64(core, mem, core->cur->sp, core->pc + 1); // Push RETA
		if (ret) break;
		core->cur->sp += STACK_FRAME_METADATA_SIZE - 8;
		core->cur->sfp = core->cur->sp;
		core->pc = temp_u64;
		break;
	case op_ret:
		ret = core_stack_read64(core, mem, core->cur->sfp - STACK_FRAME_METADATA_SIZE + 8, &temp_u64b); // Read RETA
		if (ret) break;
		ret = core_stack_read64(core, mem, core->cur->sfp - STACK_FRAME_METADATA_SIZE, &temp_u64); // Read PSFP
		if (ret) break;
		core->cur->sp = core->cur->sfp - STACK_FRAME_METADATA_SIZE;
		core->cur->sfp = temp_u64;
		core->pc = temp_u64b;
		break;
	case op_reti:
		ret = core_stack_read64(core, mem, core->cur->sfp - STACK_FRAME_METADATA_SIZE + 8, &temp_u64b); // Read RETA
		if (ret) break;
		ret = core_stack_read64(core, mem, core->cur->sfp - STACK_FRAME_METADATA_SIZE, &temp_u64); // Read PSFP
		if (ret) break;
		ret = core_stack_read8(core, mem, core->cur->sfp - STACK_FRAME_METADATA_SIZE - 1, &temp_u8); // Read PCTX
		if (ret) break;
		if (temp_u8 >= sizeof(core->scbs) / sizeof(*core->scbs)) { // Check context index for validity
			ret = STINT_BAD_CTX;
			// Note: Since we don't increment the program counter, we're likely to be stuck here forever
		}
		else {
			core->cur->sp = core->cur->sfp - STACK_FRAME_METADATA_SIZE - 2;
			core->cur->sfp = temp_u64;
			core->cur = &core->scbs[temp_u8]; // Switch back to original context
			core->pc = temp_u64b;
		}
		break;

	//
	// Jump operations
	//
	case op_jmp:
		ret = core_mem_read64(core, mem, core->pc + 1, &temp_u64); // Read imm address
		if (ret) break;
		core->pc = temp_u64;
		break;
	case op_jmps:
		ret = core_frame_read64(core, mem, core->cur->sp - 8, &temp_u64); // Read and pop address
		if (ret) break;
		core->cur->sp -= 8;
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
	// Branching operations
	//
	case op_rbrz8i8:
		ret = core_mem_read8(core, mem, core->pc + 1, &temp_u8b); // Read offset
		if (ret) break;
		ret = core_frame_read8(core, mem, core->cur->sp - 1, &temp_u8); // Read condition
		if (ret) break;
		core->cur->sp -= 1;
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
		ret = core_frame_read8(core, mem, core->cur->sp - 1, &temp_u8); // Read condition
		if (ret) break;
		core->cur->sp -= 1;
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
		ret = core_frame_read8(core, mem, core->cur->sp - 1, &temp_u8); // Read condition
		if (ret) break;
		core->cur->sp -= 1;
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
		ret = core_frame_read16(core, mem, core->cur->sp - 2, &temp_u16); // Read condition
		if (ret) break;
		core->cur->sp -= 2;
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
		ret = core_frame_read16(core, mem, core->cur->sp - 2, &temp_u16b); // Read condition
		if (ret) break;
		core->cur->sp -= 2;
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
		ret = core_frame_read16(core, mem, core->cur->sp - 2, &temp_u16); // Read condition
		if (ret) break;
		core->cur->sp -= 2;
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
		ret = core_frame_read32(core, mem, core->cur->sp - 4, &temp_u32); // Read condition
		if (ret) break;
		core->cur->sp -= 4;
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
		ret = core_frame_read32(core, mem, core->cur->sp - 4, &temp_u32); // Read condition
		if (ret) break;
		core->cur->sp -= 4;
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
		ret = core_frame_read32(core, mem, core->cur->sp - 4, &temp_u32b); // Read condition
		if (ret) break;
		core->cur->sp -= 4;
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
		ret = core_frame_read64(core, mem, core->cur->sp - 8, &temp_u64); // Read condition
		if (ret) break;
		core->cur->sp -= 8;
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
		ret = core_frame_read64(core, mem, core->cur->sp - 8, &temp_u64); // Read condition
		if (ret) break;
		core->cur->sp -= 8;
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
		ret = core_frame_read64(core, mem, core->cur->sp - 8, &temp_u64); // Read condition
		if (ret) break;
		core->cur->sp -= 8;
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
		core->pc += 1;
		ret = core_frame_read64(core, mem, core->cur->sp - 8, &temp_u64b); // Read addr
		if (ret) break;
		ret = core_mem_read8(core, mem, temp_u64b, &temp_u8); // Read data
		if (ret) break;
		ret = core_frame_write8(core, mem, core->cur->sp, temp_u8); // Write to stack
		if (ret) break;
		core->cur->sp += 1;
		break;
	case op_load16:
		core->pc += 1;
		ret = core_frame_read64(core, mem, core->cur->sp - 8, &temp_u64b); // Read addr
		if (ret) break;
		ret = core_mem_read16(core, mem, temp_u64b, &temp_u16); // Read data
		if (ret) break;
		ret = core_frame_write16(core, mem, core->cur->sp, temp_u16); // Write to stack
		if (ret) break;
		core->cur->sp += 2;
		break;
	case op_load32:
		core->pc += 1;
		ret = core_frame_read64(core, mem, core->cur->sp - 8, &temp_u64b); // Read addr
		if (ret) break;
		ret = core_mem_read32(core, mem, temp_u64b, &temp_u32); // Read data
		if (ret) break;
		ret = core_frame_write32(core, mem, core->cur->sp, temp_u32); // Write to stack
		if (ret) break;
		core->cur->sp += 4;
		break;
	case op_load64:
		core->pc += 1;
		ret = core_frame_read64(core, mem, core->cur->sp - 8, &temp_u64b); // Read addr
		if (ret) break;
		ret = core_mem_read64(core, mem, temp_u64b, &temp_u64); // Read data
		if (ret) break;
		ret = core_frame_write64(core, mem, core->cur->sp, temp_u64); // Write to stack
		if (ret) break;
		core->cur->sp += 8;
		break;
	case op_loadpop8:
		core->pc += 1;
		ret = core_frame_read64(core, mem, core->cur->sp - 8, &temp_u64b); // Read addr
		if (ret) break;
		ret = core_mem_read8(core, mem, temp_u64b, &temp_u8); // Read data
		if (ret) break;
		ret = core_frame_write8(core, mem, core->cur->sp - 8, temp_u8); // Write to stack
		if (ret) break;
		core->cur->sp -= 7;
		break;
	case op_loadpop16:
		core->pc += 1;
		ret = core_frame_read64(core, mem, core->cur->sp - 8, &temp_u64b); // Read addr
		if (ret) break;
		ret = core_mem_read16(core, mem, temp_u64b, &temp_u16); // Read data
		if (ret) break;
		ret = core_frame_write16(core, mem, core->cur->sp - 8, temp_u16); // Write to stack
		if (ret) break;
		core->cur->sp -= 6;
		break;
	case op_loadpop32:
		core->pc += 1;
		ret = core_frame_read64(core, mem, core->cur->sp - 8, &temp_u64b); // Read addr
		if (ret) break;
		ret = core_mem_read32(core, mem, temp_u64b, &temp_u32); // Read data
		if (ret) break;
		ret = core_frame_write32(core, mem, core->cur->sp - 8, temp_u32); // Write to stack
		if (ret) break;
		core->cur->sp -= 4;
		break;
	case op_loadpop64:
		core->pc += 1;
		ret = core_frame_read64(core, mem, core->cur->sp - 8, &temp_u64b); // Read addr
		if (ret) break;
		ret = core_mem_read64(core, mem, temp_u64b, &temp_u64); // Read data
		if (ret) break;
		ret = core_frame_write64(core, mem, core->cur->sp - 8, temp_u64); // Write to stack
		if (ret) break;
		break;
	case op_loadsfp8:
		core->pc += 1;
		ret = core_frame_read64(core, mem, core->cur->sp - 8, &temp_u64); // Read offset
		if (ret) break;
		if ((int64_t)temp_u64 < 0) {
			temp_u64 -= STACK_FRAME_METADATA_SIZE;
		}
		ret = core_stack_read8(core, mem, core->cur->sfp + (int64_t)temp_u64, &temp_u8); // Read data
		if (ret) break;
		ret = core_frame_write8(core, mem, core->cur->sp, temp_u8); // Write to stack
		if (ret) break;
		core->cur->sp += 1;
		break;
	case op_loadsfp16:
		core->pc += 1;
		ret = core_frame_read64(core, mem, core->cur->sp - 8, &temp_u64); // Read offset
		if (ret) break;
		if ((int64_t)temp_u64 < 0) {
			temp_u64 -= STACK_FRAME_METADATA_SIZE;
		}
		ret = core_stack_read16(core, mem, core->cur->sfp + (int64_t)temp_u64, &temp_u16); // Read data
		if (ret) break;
		ret = core_frame_write16(core, mem, core->cur->sp, temp_u16); // Write to stack
		if (ret) break;
		core->cur->sp += 2;
		break;
	case op_loadsfp32:
		core->pc += 1;
		ret = core_frame_read64(core, mem, core->cur->sp - 8, &temp_u64); // Read offset
		if (ret) break;
		if ((int64_t)temp_u64 < 0) {
			temp_u64 -= STACK_FRAME_METADATA_SIZE;
		}
		ret = core_stack_read32(core, mem, core->cur->sfp + (int64_t)temp_u64, &temp_u32); // Read data
		if (ret) break;
		ret = core_frame_write32(core, mem, core->cur->sp, temp_u32); // Write to stack
		if (ret) break;
		core->cur->sp += 4;
		break;
	case op_loadsfp64:
		core->pc += 1;
		ret = core_frame_read64(core, mem, core->cur->sp - 8, &temp_u64); // Read offset
		if (ret) break;
		if ((int64_t)temp_u64 < 0) {
			temp_u64 -= STACK_FRAME_METADATA_SIZE;
		}
		ret = core_stack_read64(core, mem, core->cur->sfp + (int64_t)temp_u64, &temp_u64b); // Read data
		if (ret) break;
		ret = core_frame_write64(core, mem, core->cur->sp, temp_u64b); // Write to stack
		if (ret) break;
		core->cur->sp += 8;
		break;
	case op_loadpopsfp8:
		core->pc += 1;
		ret = core_frame_read64(core, mem, core->cur->sp - 8, &temp_u64); // Read offset
		if (ret) break;
		if ((int64_t)temp_u64 < 0) {
			temp_u64 -= STACK_FRAME_METADATA_SIZE;
		}
		ret = core_stack_read8(core, mem, core->cur->sfp + (int64_t)temp_u64, &temp_u8); // Read data
		if (ret) break;
		ret = core_frame_write8(core, mem, core->cur->sp - 8, temp_u8); // Write to stack
		if (ret) break;
		core->cur->sp -= 7;
		break;
	case op_loadpopsfp16:
		core->pc += 1;
		ret = core_frame_read64(core, mem, core->cur->sp - 8, &temp_u64); // Read offset
		if (ret) break;
		if ((int64_t)temp_u64 < 0) {
			temp_u64 -= STACK_FRAME_METADATA_SIZE;
		}
		ret = core_stack_read16(core, mem, core->cur->sfp + (int64_t)temp_u64, &temp_u16); // Read data
		if (ret) break;
		ret = core_frame_write16(core, mem, core->cur->sp - 8, temp_u16); // Write to stack
		if (ret) break;
		core->cur->sp -= 6;
		break;
	case op_loadpopsfp32:
		core->pc += 1;
		ret = core_frame_read64(core, mem, core->cur->sp - 8, &temp_u64); // Read offset
		if (ret) break;
		if ((int64_t)temp_u64 < 0) {
			temp_u64 -= STACK_FRAME_METADATA_SIZE;
		}
		ret = core_stack_read32(core, mem, core->cur->sfp + (int64_t)temp_u64, &temp_u32); // Read data
		if (ret) break;
		ret = core_frame_write32(core, mem, core->cur->sp - 8, temp_u32); // Write to stack
		if (ret) break;
		core->cur->sp -= 4;
		break;
	case op_loadpopsfp64:
		core->pc += 1;
		ret = core_frame_read64(core, mem, core->cur->sp - 8, &temp_u64); // Read offset
		if (ret) break;
		if ((int64_t)temp_u64 < 0) {
			temp_u64 -= STACK_FRAME_METADATA_SIZE;
		}
		ret = core_stack_read64(core, mem, core->cur->sfp + (int64_t)temp_u64, &temp_u64b); // Read data
		if (ret) break;
		ret = core_frame_write64(core, mem, core->cur->sp - 8, temp_u64b); // Write to stack
		if (ret) break;
		break;
	case op_store8:
		core->pc += 1;
		ret = core_frame_read8(core, mem, core->cur->sp - 1, &temp_u8); // Read data
		if (ret) break;
		ret = core_frame_read64(core, mem, core->cur->sp - 9, &temp_u64b); // Read addr
		if (ret) break;
		ret = core_mem_write8(core, mem, temp_u64b, temp_u8); // Write to memory
		if (ret) break;
		break;
	case op_store16:
		core->pc += 1;
		ret = core_frame_read16(core, mem, core->cur->sp - 2, &temp_u16); // Read data
		if (ret) break;
		ret = core_frame_read64(core, mem, core->cur->sp - 10, &temp_u64b); // Read addr
		if (ret) break;
		ret = core_mem_write16(core, mem, temp_u64b, temp_u16); // Write to memory
		if (ret) break;
		break;
	case op_store32:
		core->pc += 1;
		ret = core_frame_read32(core, mem, core->cur->sp - 4, &temp_u32); // Read data
		if (ret) break;
		ret = core_frame_read64(core, mem, core->cur->sp - 12, &temp_u64b); // Read addr
		if (ret) break;
		ret = core_mem_write32(core, mem, temp_u64b, temp_u32); // Write to memory
		if (ret) break;
		break;
	case op_store64:
		core->pc += 1;
		ret = core_frame_read64(core, mem, core->cur->sp - 8, &temp_u64); // Read data
		if (ret) break;
		ret = core_frame_read64(core, mem, core->cur->sp - 16, &temp_u64b); // Read addr
		if (ret) break;
		ret = core_mem_write64(core, mem, temp_u64b, temp_u64); // Write to memory
		if (ret) break;
		break;
	case op_storepop8:
		core->pc += 1;
		ret = core_frame_read8(core, mem, core->cur->sp - 1, &temp_u8); // Read data
		if (ret) break;
		ret = core_frame_read64(core, mem, core->cur->sp - 9, &temp_u64b); // Read addr
		if (ret) break;
		ret = core_mem_write8(core, mem, temp_u64b, temp_u8); // Write to memory
		if (ret) break;
		core->cur->sp -= 1;
		break;
	case op_storepop16:
		core->pc += 1;
		ret = core_frame_read16(core, mem, core->cur->sp - 2, &temp_u16); // Read data
		if (ret) break;
		ret = core_frame_read64(core, mem, core->cur->sp - 10, &temp_u64b); // Read addr
		if (ret) break;
		ret = core_mem_write16(core, mem, temp_u64b, temp_u16); // Write to memory
		if (ret) break;
		core->cur->sp -= 2;
		break;
	case op_storepop32:
		core->pc += 1;
		ret = core_frame_read32(core, mem, core->cur->sp - 4, &temp_u32); // Read data
		if (ret) break;
		ret = core_frame_read64(core, mem, core->cur->sp - 12, &temp_u64b); // Read addr
		if (ret) break;
		ret = core_mem_write32(core, mem, temp_u64b, temp_u32); // Write to memory
		if (ret) break;
		core->cur->sp -= 4;
		break;
	case op_storepop64:
		core->pc += 1;
		ret = core_frame_read64(core, mem, core->cur->sp - 8, &temp_u64); // Read data
		if (ret) break;
		ret = core_frame_read64(core, mem, core->cur->sp - 16, &temp_u64b); // Read addr
		if (ret) break;
		ret = core_mem_write64(core, mem, temp_u64b, temp_u64); // Write to memory
		if (ret) break;
		core->cur->sp -= 8;
		break;
	case op_storesfp8:
		core->pc += 1;
		ret = core_frame_read8(core, mem, core->cur->sp - 1, &temp_u8); // Read data
		if (ret) break;
		ret = core_frame_read64(core, mem, core->cur->sp - 9, &temp_u64); // Read offset
		if (ret) break;
		if ((int64_t)temp_u64 < 0) {
			temp_u64 -= STACK_FRAME_METADATA_SIZE;
		}
		ret = core_stack_write8(core, mem, core->cur->sfp + (int64_t)temp_u64, temp_u8); // Write to stack
		if (ret) break;
		break;
	case op_storesfp16:
		core->pc += 1;
		ret = core_frame_read16(core, mem, core->cur->sp - 2, &temp_u16); // Read data
		if (ret) break;
		ret = core_frame_read64(core, mem, core->cur->sp - 10, &temp_u64); // Read offset
		if (ret) break;
		if ((int64_t)temp_u64 < 0) {
			temp_u64 -= STACK_FRAME_METADATA_SIZE;
		}
		ret = core_stack_write16(core, mem, core->cur->sfp + (int64_t)temp_u64, temp_u16); // Write to stack
		if (ret) break;
		break;
	case op_storesfp32:
		core->pc += 1;
		ret = core_frame_read32(core, mem, core->cur->sp - 4, &temp_u32); // Read data
		if (ret) break;
		ret = core_frame_read64(core, mem, core->cur->sp - 12, &temp_u64); // Read offset
		if (ret) break;
		if ((int64_t)temp_u64 < 0) {
			temp_u64 -= STACK_FRAME_METADATA_SIZE;
		}
		ret = core_stack_write32(core, mem, core->cur->sfp + (int64_t)temp_u64, temp_u32); // Write to stack
		if (ret) break;
		break;
	case op_storesfp64:
		core->pc += 1;
		ret = core_frame_read64(core, mem, core->cur->sp - 8, &temp_u64b); // Read data
		if (ret) break;
		ret = core_frame_read64(core, mem, core->cur->sp - 16, &temp_u64); // Read offset
		if (ret) break;
		if ((int64_t)temp_u64 < 0) {
			temp_u64 -= STACK_FRAME_METADATA_SIZE;
		}
		ret = core_stack_write64(core, mem, core->cur->sfp + (int64_t)temp_u64, temp_u64b); // Write to stack
		if (ret) break;
		break;
	case op_storepopsfp8:
		core->pc += 1;
		ret = core_frame_read8(core, mem, core->cur->sp - 1, &temp_u8); // Read data
		if (ret) break;
		ret = core_frame_read64(core, mem, core->cur->sp - 9, &temp_u64); // Read offset
		if (ret) break;
		if ((int64_t)temp_u64 < 0) {
			temp_u64 -= STACK_FRAME_METADATA_SIZE;
		}
		ret = core_stack_write8(core, mem, core->cur->sfp + (int64_t)temp_u64, temp_u8); // Write to stack
		if (ret) break;
		core->cur->sp -= 1;
		break;
	case op_storepopsfp16:
		core->pc += 1;
		ret = core_frame_read16(core, mem, core->cur->sp - 2, &temp_u16); // Read data
		if (ret) break;
		ret = core_frame_read64(core, mem, core->cur->sp - 10, &temp_u64); // Read offset
		if (ret) break;
		if ((int64_t)temp_u64 < 0) {
			temp_u64 -= STACK_FRAME_METADATA_SIZE;
		}
		ret = core_stack_write16(core, mem, core->cur->sfp + (int64_t)temp_u64, temp_u16); // Write to stack
		if (ret) break;
		core->cur->sp -= 2;
		break;
	case op_storepopsfp32:
		core->pc += 1;
		ret = core_frame_read32(core, mem, core->cur->sp - 4, &temp_u32); // Read data
		if (ret) break;
		ret = core_frame_read64(core, mem, core->cur->sp - 12, &temp_u64); // Read offset
		if (ret) break;
		if ((int64_t)temp_u64 < 0) {
			temp_u64 -= STACK_FRAME_METADATA_SIZE;
		}
		ret = core_stack_write32(core, mem, core->cur->sfp + (int64_t)temp_u64, temp_u32); // Write to stack
		if (ret) break;
		core->cur->sp -= 4;
		break;
	case op_storepopsfp64:
		core->pc += 1;
		ret = core_frame_read64(core, mem, core->cur->sp - 8, &temp_u64b); // Read data
		if (ret) break;
		ret = core_frame_read64(core, mem, core->cur->sp - 16, &temp_u64); // Read offset
		if (ret) break;
		if ((int64_t)temp_u64 < 0) {
			temp_u64 -= STACK_FRAME_METADATA_SIZE;
		}
		ret = core_stack_write64(core, mem, core->cur->sfp + (int64_t)temp_u64, temp_u64b); // Write to stack
		if (ret) break;
		core->cur->sp -= 8;
		break;
	case op_storer8:
		core->pc += 1;
		ret = core_frame_read64(core, mem, core->cur->sp - 8, &temp_u64b); // Read addr
		if (ret) break;
		ret = core_frame_read8(core, mem, core->cur->sp - 9, &temp_u8); // Read data
		if (ret) break;
		ret = core_mem_write8(core, mem, temp_u64b, temp_u8); // Write to memory
		if (ret) break;
		break;
	case op_storer16:
		core->pc += 1;
		ret = core_frame_read64(core, mem, core->cur->sp - 8, &temp_u64b); // Read addr
		if (ret) break;
		ret = core_frame_read16(core, mem, core->cur->sp - 10, &temp_u16); // Read data
		if (ret) break;
		ret = core_mem_write16(core, mem, temp_u64b, temp_u16); // Write to memory
		if (ret) break;
		break;
	case op_storer32:
		core->pc += 1;
		ret = core_frame_read64(core, mem, core->cur->sp - 8, &temp_u64b); // Read addr
		if (ret) break;
		ret = core_frame_read32(core, mem, core->cur->sp - 12, &temp_u32); // Read data
		if (ret) break;
		ret = core_mem_write32(core, mem, temp_u64b, temp_u32); // Write to memory
		if (ret) break;
		break;
	case op_storer64:
		core->pc += 1;
		ret = core_frame_read64(core, mem, core->cur->sp - 8, &temp_u64b); // Read addr
		if (ret) break;
		ret = core_frame_read64(core, mem, core->cur->sp - 16, &temp_u64); // Read data
		if (ret) break;
		ret = core_mem_write64(core, mem, temp_u64b, temp_u64); // Write to memory
		if (ret) break;
		break;
	case op_storerpop8:
		core->pc += 1;
		ret = core_frame_read64(core, mem, core->cur->sp - 8, &temp_u64b); // Read addr
		if (ret) break;
		ret = core_frame_read8(core, mem, core->cur->sp - 9, &temp_u8); // Read data
		if (ret) break;
		ret = core_mem_write8(core, mem, temp_u64b, temp_u8); // Write to memory
		if (ret) break;
		core->cur->sp -= 8;
		break;
	case op_storerpop16:
		core->pc += 1;
		ret = core_frame_read64(core, mem, core->cur->sp - 8, &temp_u64b); // Read addr
		if (ret) break;
		ret = core_frame_read16(core, mem, core->cur->sp - 10, &temp_u16); // Read data
		if (ret) break;
		ret = core_mem_write16(core, mem, temp_u64b, temp_u16); // Write to memory
		if (ret) break;
		core->cur->sp -= 8;
		break;
	case op_storerpop32:
		core->pc += 1;
		ret = core_frame_read64(core, mem, core->cur->sp - 8, &temp_u64b); // Read addr
		if (ret) break;
		ret = core_frame_read32(core, mem, core->cur->sp - 12, &temp_u32); // Read data
		if (ret) break;
		ret = core_mem_write32(core, mem, temp_u64b, temp_u32); // Write to memory
		if (ret) break;
		core->cur->sp -= 8;
		break;
	case op_storerpop64:
		core->pc += 1;
		ret = core_frame_read64(core, mem, core->cur->sp - 8, &temp_u64b); // Read addr
		if (ret) break;
		ret = core_frame_read64(core, mem, core->cur->sp - 16, &temp_u64); // Read data
		if (ret) break;
		ret = core_mem_write64(core, mem, temp_u64b, temp_u64); // Write to memory
		if (ret) break;
		core->cur->sp -= 8;
		break;
	case op_storersfp8:
		core->pc += 1;
		ret = core_frame_read64(core, mem, core->cur->sp - 8, &temp_u64); // Read offset
		if (ret) break;
		ret = core_frame_read8(core, mem, core->cur->sp - 9, &temp_u8); // Read data
		if (ret) break;
		if ((int64_t)temp_u64 < 0) {
			temp_u64 -= STACK_FRAME_METADATA_SIZE;
		}
		ret = core_stack_write8(core, mem, core->cur->sfp + (int64_t)temp_u64, temp_u8); // Write to stack
		if (ret) break;
		break;
	case op_storersfp16:
		core->pc += 1;
		ret = core_frame_read64(core, mem, core->cur->sp - 8, &temp_u64); // Read offset
		if (ret) break;
		ret = core_frame_read16(core, mem, core->cur->sp - 10, &temp_u16); // Read data
		if (ret) break;
		if ((int64_t)temp_u64 < 0) {
			temp_u64 -= STACK_FRAME_METADATA_SIZE;
		}
		ret = core_stack_write16(core, mem, core->cur->sfp + (int64_t)temp_u64, temp_u16); // Write to stack
		if (ret) break;
		break;
	case op_storersfp32:
		core->pc += 1;
		ret = core_frame_read64(core, mem, core->cur->sp - 8, &temp_u64); // Read offset
		if (ret) break;
		ret = core_frame_read32(core, mem, core->cur->sp - 12, &temp_u32); // Read data
		if (ret) break;
		if ((int64_t)temp_u64 < 0) {
			temp_u64 -= STACK_FRAME_METADATA_SIZE;
		}
		ret = core_stack_write32(core, mem, core->cur->sfp + (int64_t)temp_u64, temp_u32); // Write to stack
		if (ret) break;
		break;
	case op_storersfp64:
		core->pc += 1;
		ret = core_frame_read64(core, mem, core->cur->sp - 8, &temp_u64); // Read offset
		if (ret) break;
		ret = core_frame_read64(core, mem, core->cur->sp - 16, &temp_u64b); // Read data
		if (ret) break;
		if ((int64_t)temp_u64 < 0) {
			temp_u64 -= STACK_FRAME_METADATA_SIZE;
		}
		ret = core_stack_write64(core, mem, core->cur->sfp + (int64_t)temp_u64, temp_u64b); // Write to stack
		if (ret) break;
		break;
	case op_storerpopsfp8:
		core->pc += 1;
		ret = core_frame_read64(core, mem, core->cur->sp - 8, &temp_u64); // Read offset
		if (ret) break;
		ret = core_frame_read8(core, mem, core->cur->sp - 9, &temp_u8); // Read data
		if (ret) break;
		if ((int64_t)temp_u64 < 0) {
			temp_u64 -= STACK_FRAME_METADATA_SIZE;
		}
		ret = core_stack_write8(core, mem, core->cur->sfp + (int64_t)temp_u64, temp_u8); // Write to stack
		if (ret) break;
		core->cur->sp -= 8;
		break;
	case op_storerpopsfp16:
		core->pc += 1;
		ret = core_frame_read64(core, mem, core->cur->sp - 8, &temp_u64); // Read offset
		if (ret) break;
		ret = core_frame_read16(core, mem, core->cur->sp - 10, &temp_u16); // Read data
		if (ret) break;
		if ((int64_t)temp_u64 < 0) {
			temp_u64 -= STACK_FRAME_METADATA_SIZE;
		}
		ret = core_stack_write16(core, mem, core->cur->sfp + (int64_t)temp_u64, temp_u16); // Write to stack
		if (ret) break;
		core->cur->sp -= 8;
		break;
	case op_storerpopsfp32:
		core->pc += 1;
		ret = core_frame_read64(core, mem, core->cur->sp - 8, &temp_u64); // Read offset
		if (ret) break;
		ret = core_frame_read32(core, mem, core->cur->sp - 12, &temp_u32); // Read data
		if (ret) break;
		if ((int64_t)temp_u64 < 0) {
			temp_u64 -= STACK_FRAME_METADATA_SIZE;
		}
		ret = core_stack_write32(core, mem, core->cur->sfp + (int64_t)temp_u64, temp_u32); // Write to stack
		if (ret) break;
		core->cur->sp -= 8;
		break;
	case op_storerpopsfp64:
		core->pc += 1;
		ret = core_frame_read64(core, mem, core->cur->sp - 8, &temp_u64); // Read offset
		if (ret) break;
		ret = core_frame_read64(core, mem, core->cur->sp - 16, &temp_u64b); // Read data
		if (ret) break;
		if ((int64_t)temp_u64 < 0) {
			temp_u64 -= STACK_FRAME_METADATA_SIZE;
		}
		ret = core_stack_write64(core, mem, core->cur->sfp + (int64_t)temp_u64, temp_u64b); // Write to stack
		if (ret) break;
		core->cur->sp -= 8;
		break;

	//
	// Special Operations
	//
	case op_pushsfp:
		core->pc += 1;
		ret = core_frame_write64(core, mem, core->cur->sp, core->cur->sfp); // Write to stack
		if (ret) break;
		core->cur->sp += 8;
		break;
	case op_setsbp:
		core->pc += 9;
		ret = core_mem_read64(core, mem, core->pc - 8, &temp_u64); // Read addr
		if (ret) break;
		core->cur->sbp = temp_u64;
		break;
	case op_setsfp:
		core->pc += 9;
		ret = core_mem_read64(core, mem, core->pc - 8, &temp_u64); // Read addr
		if (ret) break;
		core->cur->sfp = temp_u64;
		break;
	case op_setslp:
		core->pc += 9;
		ret = core_mem_read64(core, mem, core->pc - 8, &temp_u64); // Read addr
		if (ret) break;
		core->cur->slp = temp_u64;
		break;
	case op_setsp:
		core->pc += 9;
		ret = core_mem_read64(core, mem, core->pc - 8, &temp_u64); // Read addr
		if (ret) break;
		core->cur->sp = temp_u64;
		break;
	case op_incctx:
		core->pc += 1;
		if (core->cur < core->scbs + sizeof(core->scbs) / sizeof(*core->scbs) - 1) {
			core->cur++;
		}
		else {
			ret = STINT_BAD_CTX;
		}
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
		core->pc += 1;
		ret = STINT_INVALID_INST;
		break;
	}

	if (ret > 0 && ret < 256) {
		// An interrupt occurred. Switch to the supervisor stack, push the interrupt number,
		// previous context index, SFP, then PC to the stack, then vector to the interrupt handler.
		// Note: We ignore any errors that occur pushing to the stack because
		// an attempt to handle them would likely generate more errors.
		struct scb *pscb = core->cur;
		core->cur = core->scbs; // Select supervisor context
		core_frame_write8(core, mem, core->cur->sp, ret);
		core->cur->sp += 1;
		core_frame_write8(core, mem, core->cur->sp, (uint8_t)(pscb - core->scbs));
		core->cur->sp += 1;
		core_frame_write64(core, mem, core->cur->sp, core->cur->sfp);
		core->cur->sp += 8;
		core_frame_write64(core, mem, core->cur->sp, core->pc);
		core->cur->sp += 8;
		core->cur->sfp = core->cur->sp;
		core->pc = BEGIN_INT_ADDR + 16 * ret;
	}

	return ret;
}
