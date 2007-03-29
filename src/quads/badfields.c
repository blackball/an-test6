/*
  This file is part of the Astrometry.net suite.
  Copyright 2006, Dustin Lang, Keir Mierle and Sam Roweis.

  The Astrometry.net suite is free software; you can redistribute
  it and/or modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation, version 2.

  The Astrometry.net suite is distributed in the hope that it will be
  useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with the Astrometry.net suite ; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "solvedfile.h"

struct badfield {
	int run;
	int firstfield;
	int lastfield;
	char* problem;
	char* comments;
};
typedef struct badfield badfield;

//static badfield baddies[] = {
//#include "badfields.inc"
//};

const char* OPTIONS = "hs:";

void printHelp(char* progname) {
	fprintf(stderr, "Usage: %s\n"
			"   [-s <solved-file-template>]\n"
			"\n", progname);
}

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char** args) {
    int argchar;
	char* progname = args[0];
	int i;
	int NB = sizeof(baddies) / sizeof(badfield);
	char* solvedtemplate = NULL;

    while ((argchar = getopt (argc, args, OPTIONS)) != -1) {
		switch (argchar) {
		case 's':
			solvedtemplate = optarg;
			break;
		case 'h':
		default:
			printHelp(progname);
			exit(-1);
		}
	}

	printf("Number of bad field entries: %i.\n", NB);
	/*
	  for (i=0; i<NB; i++) {
	  printf("   run %i, fields %i-%i.\n", baddies[i].run, baddies[i].firstfield, baddies[i].lastfield);
	  }
	*/

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
				   &filenum, &fieldnum, &run, &rerun, 
				   &camcol, &field, &filter, &ifield) != 8) {
			fprintf(stderr, "Failed to parse line: %s\n", line);
			continue;
		}
		for (i=0; i<NB; i++) {
			badfield* bad = baddies + i;
			if (!((run == bad->run) &&
				  (field >= bad->firstfield) &&
				  (field <= bad->lastfield)))
				continue;
			if (solvedtemplate) {
				char fn[256];
				sprintf(fn, solvedtemplate, filenum);
				if (solvedfile_get(fn, fieldnum) == 0) {
					printf("%i %i %i %i \"%s\" \"%s\"\n", filenum, fieldnum, run, field,
						   bad->problem, bad->comments);
				}
			} else {
				printf("%i %i %i %i \"%s\" \"%s\"\n", filenum, fieldnum, run, field,
					   bad->problem, bad->comments);
			}
		}
	}

	return 0;
}
