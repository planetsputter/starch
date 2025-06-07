// core.h
//
// Represents a Starch processor core

#pragma once

#include <stdint.h>
#include "mem.h"

struct core {
	uint64_t pc, sbp, sfp, sp, slp;
};

void core_init(struct core*);
void core_destroy(struct core*);

// Executes a single instruction on the core based on the given memory
int core_step(struct core*, struct mem*);
