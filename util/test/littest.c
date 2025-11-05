// littest.c

#include <assert.h>

#include "bstr.h"
#include "lits.h"

// Character literal test case
struct cltc {
	bool valid;
	const char *lit;
	ucp val;
};

struct cltc cltcs[] = {
	{ false, "0", 0 },
	{ true, "'0'", '0' },
	{ false, "'00'", 0 },
	{ true, "'\a'", '\a' },
	{ false, "'\a0'", 0 },
};

int main()
{
	// Run all character literal test cases
	for (size_t i = 0; i < sizeof(cltcs) / sizeof(*cltcs); i++) {
		bchar *bstr = bstrdupc(cltcs[i].lit);
		ucp val = cltcs[i].val + 1;
		bool ret = parse_char_lit(bstr, &val);
		bfree(bstr);
		assert(ret == cltcs[i].valid);
		if (ret) {
			assert(val == cltcs[i].val);
		}
		else {
			assert(val == cltcs[i].val + 1);
		}
	}

	return 0;
}
