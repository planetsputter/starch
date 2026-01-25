// smap.c

#include <string.h>
#include <stdlib.h>

// Map for C-strings
#define MNAME smap
#define KEYT char*
#define VALT char*
#define COMPF strcmp
#define KEYDELF free
#define VALDELF free
#include "map.ct"
#undef MNAME
#undef KEYT
#undef VALT
#undef COMPF
#undef KEYDELF
#undef VALDELF
