#include <stdio.h>
#include "sz.h"

/* This trivial program attempts to read stdin, then write it out
 * to stdout.  It's probably very inefficient; it's just intended to
 * show how the library works.
 */
int
main(void) {
	sz *s;

	while (s = szfread(stdin, "\n\r")) {
		szccat(s, '\n');
		szwrite(s);
		szfree(s);
	}
	return 0;
}
