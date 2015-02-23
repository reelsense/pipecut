#include <stdio.h>
#include <stdlib.h>

#include "version.h"

void
version(void) {
	fprintf(stderr, "sz revision %s\n", SZ_REV);
	exit(EXIT_SUCCESS);
}

