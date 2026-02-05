// stem.h
// Global declarations for stem

#pragma once

#include "bpmap.h"
#include "core.h"
#include "mem.h"

// The number of cores in the Starch virtual machine
enum { STEM_NUM_CORES = 1 }; // Currently single-core

// Array of Starch virtual machine cores.
// The first element of the array is the main core.
extern struct core cores[STEM_NUM_CORES];

// The main emulated memory
extern struct mem main_mem;

// Breakpoint map
extern struct bpmap *bpmap;

