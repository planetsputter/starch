// bpmap.c

#include "bpmap.h"

#include <string.h>

// Simple function to compare addresses for sorting
static int comp_addr(uint64_t left, uint64_t right)
{
	return left < right ? -1 : left == right ? 0 : 1;
}

// Map for breakpoints
#define MNAME bpmap
#define KEYT uint64_t // Address
#define VALT int // Count
#define COMPF comp_addr
#define KEYDELF (void)
#define VALDELF (void)
#include "map.ct"
#undef MNAME
#undef KEYT
#undef VALT
#undef COMPF
#undef KEYDELF
#undef VALDELF
