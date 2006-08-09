#include <stdio.h>
#include <stdlib.h>

struct badfield {
	int run;
	int firstfield;
	int lastfield;
	char* problem;
	char* comments;
};
typedef struct badfield badfield;

static badfield baddies[] = {
#include "badfields.inc"
};

int main(int argc, char** args) {
	int i;
	int NB = sizeof(baddies) / sizeof(badfield);

	printf("Number of bad field entries: %i.\n", NB);
	for (i=0; i<NB; i++) {
		printf("   run %i, fields %i-%i.\n", baddies[i].run, baddies[i].firstfield, baddies[i].lastfield);
	}

	for (;;) {
		char line[256];
		int filenum, fieldnum, run, field, rerun, camcol, filter, ifield;
		if (!fgets(line, 256, stdin)) {
			if (feof(stdin))
				break;
			fprintf(stderr, "Error reading a line of input from stdin.\n");
			exit(-1);
		}
		if (sscanf(line, "%i %i %i %i %i %i %i %i\n",
				   &filenum, &fieldnum, &run, &field,
				   &rerun, &camcol, &filter, &ifield) != 8) {
			fprintf(stderr, "Failed to parse line: %s\n", line);
			continue;
		}
		for (i=0; i<NB; i++) {
			badfield* bad = baddies + i;
			if (!((run == bad->run) &&
				  (field >= bad->firstfield) &&
				  (field <= bad->lastfield)))
				continue;
			printf("%i %i %i %i \"%s\" \"%s\"\n", filenum, fieldnum, run, field,
				   bad->problem, bad->comments);
		}
	}

	return 0;
}
