#include "sz.h"
#include <stdio.h>
#include <stdlib.h>

int
main(int argc, char **argv) {
	sz *f, *t, *line;
	char buf[1024];

	if (argc < 3) {
		fprintf(stderr, "usage: sztr pat1 pat2\n");
		exit(EXIT_FAILURE);
	}

	f = str_decode(argv[1]);
	t = str_decode(argv[2]);
	while (fgets(buf, 1024, stdin)) {
		line = str2zsz(buf);
		sztr(line, f, t);
		fputs(buf, stdout);
		szfree(line);
	}
	return 0;
}
