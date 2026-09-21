// core.h
//
// Represents a Starch processor core

#pragma once

#include <stdint.h>
#include "mem.h"

// Stack control block
struct scb {
	uint64_t pc, sbp, sfp, sp, slp;
};

// Starch processor core
struct core {
	// Supervisor and user stack control blocks
	struct scb scbs[2];
	struct scb *cur; // Pointer to current stack control block, performing function of CTX register

	// Program counter
	uint64_t pc;

	// Buffers for stdin and stdout
	uint8_t *stdin_buff, *stdout_buff;
	int stdin_head, stdin_tail, stdout_count;
};

void core_init(struct core*);
void core_destroy(struct core*);

// Executes a single instruction on the core based on the given memory.
// A negative return value indicates an error in the emulator.
// A return value of zero indicates the instruction completed without interrupt.
// A positive return value less than 256 indicates an interrupt occurred.
// A return value greater than or equal to 256 indicates the core should halt.
// The halt code is 256 less than the return value.
int core_step(struct core*, struct mem*);
