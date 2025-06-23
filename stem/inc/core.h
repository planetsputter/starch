// core.h
//
// Represents a Starch processor core

#pragma once

#include <stdint.h>
#include "mem.h"

struct core {
	// The core Starch registers
	uint64_t pc, sbp, sfp, sp, slp;

	// Buffers for stdin and stdout
	uint8_t *stdin_buff, *stdout_buff;
	int stdin_head, stdin_tail, stdout_count;
};

void core_init(struct core*);
void core_destroy(struct core*);

// Executes a single instruction on the core based on the given memory
int core_step(struct core*, struct mem*);
