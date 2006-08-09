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

	printf("NB %i.\n", NB);
	for (i=0; i<NB; i++) {
		printf("run %i, fields %i-%i.\n", baddies[i].run, baddies[i].firstfield, baddies[i].lastfield);
	}

	return 0;
}
