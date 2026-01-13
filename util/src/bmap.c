// bmap.c

#include <string.h>
#include "bstr.h"

// Map for B-strings
#define MNAME bmap
#define KEYT bchar*
#define VALT bchar*
#define COMPF bstrcmpb
#define KEYDELF bfree
#define VALDELF bfree
#include "map.ct"
#undef MNAME
#undef KEYT
#undef VALT
#undef COMPF
#undef KEYDELF
#undef VALDELF
