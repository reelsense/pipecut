#include "../sz.h"
#include <assert.h>

int
main(void) {
	sz *s = szgetp("foo");
	sz *t;
	assert(szlen(s) == 3);
	assert(t = szchr(s, 'f'));
	assert(szlen(t) == 3);
	assert(szdata(s) == szdata(t));
	szfree(s);
	return szstats();
}
