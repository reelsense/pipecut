#include "../sz.h"
#include <assert.h>
#include <string.h>

int
main(void) {
	sz *s = szgetp("foo");
	assert(szlen(s) == 3);
	assert(!memcmp(szdata(s), "foo", 4));
	szchr(s, 'f');
	szfree(s);
	return szstats();
}
