// core.h

#pragma once

#include <stdint.h>
#include "mem.h"

struct core {
	uint64_t pc, sbp, sfp, sp;
};

void core_init(struct core*);
void core_destroy(struct core*);

int core_step(struct core*, struct mem*);
